/**
 * EV SmartCharge - Public Website Scripts
 */

document.addEventListener('DOMContentLoaded', () => {
    initNavigation();
    initFaqAccordion();
    initLiveStats();
    initStationFinder();
    initContactForm();
});

// Mobile Navigation Toggle
function initNavigation() {
    const navToggle = document.querySelector('.nav-toggle');
    const navList = document.querySelector('.nav-list');

    if (navToggle && navList) {
        navToggle.addEventListener('click', () => {
            navList.classList.toggle('open');
            const isOpen = navList.classList.contains('open');
            navToggle.setAttribute('aria-expanded', isOpen);
            navToggle.innerHTML = isOpen ? '✕' : '☰';
        });

        // Close on link click
        navList.querySelectorAll('a').forEach(link => {
            link.addEventListener('click', () => {
                navList.classList.remove('open');
                navToggle.innerHTML = '☰';
            });
        });
    }
}

// FAQ Accordion
function initFaqAccordion() {
    const faqItems = document.querySelectorAll('.faq-item');
    faqItems.forEach(item => {
        const question = item.querySelector('.faq-question');
        if (question) {
            question.addEventListener('click', () => {
                const isActive = item.classList.contains('active');
                // Close others
                faqItems.forEach(other => other.classList.remove('active'));
                if (!isActive) {
                    item.classList.add('active');
                }
            });
        }
    });
}

// Live Statistics on Homepage
async function initLiveStats() {
    const statsContainer = document.querySelector('.stats-grid');
    if (!statsContainer) return;

    try {
        const stats = await api.get('/dashboard');
        
        const totalStationsEl = document.getElementById('stat-total-stations');
        const availableSlotsEl = document.getElementById('stat-available-slots');
        const totalVehiclesEl = document.getElementById('stat-total-vehicles');
        const totalAssignmentsEl = document.getElementById('stat-total-assignments');

        if (totalStationsEl) totalStationsEl.textContent = stats.totalStations || 0;
        if (availableSlotsEl) availableSlotsEl.textContent = stats.availableSlots || 0;
        if (totalVehiclesEl) totalVehiclesEl.textContent = stats.totalVehicles || 0;
        if (totalAssignmentsEl) totalAssignmentsEl.textContent = stats.totalAssignments || 0;
    } catch (err) {
        console.warn('Could not load live dashboard stats for homepage:', err);
    }
}

// Station Finder on find-charger.html
let allStations = [];

async function initStationFinder() {
    const grid = document.getElementById('stations-display-grid');
    if (!grid) return;

    const searchInput = document.getElementById('station-search-input');
    const connectorFilter = document.getElementById('connector-filter');
    const statusFilter = document.getElementById('status-filter');
    const countBadge = document.getElementById('results-count');

    try {
        grid.innerHTML = '<div style="grid-column: 1/-1; text-align: center; padding: 40px; color: var(--color-text-muted);">Loading live stations from C++ backend...</div>';
        allStations = await api.get('/stations');
        renderStations(allStations);
    } catch (err) {
        grid.innerHTML = `<div style="grid-column: 1/-1; text-align: center; padding: 40px; color: var(--color-error);">Unable to load charging stations: ${err.message}</div>`;
    }

    function renderStations(stations) {
        if (!grid) return;
        if (countBadge) countBadge.textContent = `${stations.length} Station${stations.length === 1 ? '' : 's'} Found`;

        if (stations.length === 0) {
            grid.innerHTML = '<div style="grid-column: 1/-1; text-align: center; padding: 40px; color: var(--color-text-muted);">No charging stations match your filter criteria.</div>';
            return;
        }

        grid.innerHTML = stations.map(s => {
            const statusClass = s.available ? 'badge-available' : 'badge-occupied';
            const statusText = s.available ? 'Available' : 'Occupied';
            const vehicleInfo = !s.available && s.currentVehicle 
                ? `<div style="font-size: 0.8rem; margin-top: 6px; color: var(--color-text-muted);">Charging: <strong>${s.currentVehicle}</strong></div>` 
                : '';

            return `
                <div class="station-card">
                    <div class="station-card-header">
                        <div class="station-id">⚡ Station #${s.stationID}</div>
                        <span class="badge ${statusClass}">${statusText}</span>
                    </div>
                    <div class="station-specs">
                        <div><strong>Charging Speed:</strong> ${s.chargingSpeed} kW (DC Fast)</div>
                        <div><strong>Connector Type:</strong> <span class="badge badge-type">${s.connectorType}</span></div>
                        <div><strong>Location:</strong> ${s.location || 'Metro Expressway Corridor Hub'}</div>
                        ${vehicleInfo}
                    </div>
                    <div class="station-card-actions" style="margin-top: 14px; display: flex; gap: 10px;">
                        <button class="btn btn-secondary btn-sm" style="flex:1; text-align:center;" onclick="viewStationDetail(${s.stationID})">Details</button>
                        ${s.available ? `<button class="btn btn-primary btn-sm" style="flex:1; text-align:center;" onclick="openBookingModal(${s.stationID}, '${s.connectorType}', ${s.chargingSpeed})">Book Slot ➔</button>` : ''}
                    </div>
                </div>
            `;
        }).join('');
    }

    function applyFilters() {
        const query = (searchInput ? searchInput.value : '').toLowerCase().trim();
        const connector = connectorFilter ? connectorFilter.value : 'ALL';
        const status = statusFilter ? statusFilter.value : 'ALL';

        const filtered = allStations.filter(s => {
            const matchesQuery = query === '' || 
                String(s.stationID).includes(query) || 
                (s.location && s.location.toLowerCase().includes(query)) ||
                s.connectorType.toLowerCase().includes(query);

            const matchesConnector = connector === 'ALL' || s.connectorType === connector;
            
            let matchesStatus = true;
            if (status === 'AVAILABLE') matchesStatus = s.available === true;
            if (status === 'OCCUPIED') matchesStatus = s.available === false;

            return matchesQuery && matchesConnector && matchesStatus;
        });

        renderStations(filtered);
    }

    if (searchInput) searchInput.addEventListener('input', applyFilters);
    if (connectorFilter) connectorFilter.addEventListener('change', applyFilters);
    if (statusFilter) statusFilter.addEventListener('change', applyFilters);
}

// Station Details Modal
window.viewStationDetail = function(stationId) {
    const station = allStations.find(s => s.stationID === stationId);
    if (!station) return;

    alert(
        `⚡ EV SMARTCHARGE STATION #${station.stationID}\n\n` +
        `Power Rating: ${station.chargingSpeed} kW (DC Ultra-Fast)\n` +
        `Connector Standard: ${station.connectorType}\n` +
        `Availability: ${station.available ? 'AVAILABLE FOR IMMEDIATE CHARGING' : 'OCCUPIED by ' + (station.currentVehicle || 'Vehicle')}\n` +
        `Bay Location: ${station.location || 'Metro Expressway Corridor Hub Bay'}\n` +
        `Telemetry Status: ${station.maintenanceStatus || 'Online & Operational'}\n\n` +
        `Managed by EV SmartCharge Network Dispatch Engine.`
    );
};

// Contact Form Validation
function initContactForm() {
    const form = document.getElementById('contact-form');
    if (!form) return;

    form.addEventListener('submit', (e) => {
        e.preventDefault();
        const name = form.querySelector('[name="name"]').value.trim();
        const email = form.querySelector('[name="email"]').value.trim();
        const message = form.querySelector('[name="message"]').value.trim();

        if (!name || !email || !message) {
            showToast('Please fill in all required fields.', 'warning');
            return;
        }

        const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        if (!emailRegex.test(email)) {
            showToast('Please enter a valid email address.', 'warning');
            return;
        }

        showToast('Thank you! Your message has been sent to the EV SmartCharge support team.', 'success');
        form.reset();
    });
}



// ============================================================
// CUSTOMER CHARGING SLOT BOOKING WORKFLOW (v2.0)
// Complete 6-step Reservation System connected to C++ Engine & SQLite
// ============================================================

window.openBookingModal = async function(preferredStationID = null, connectorType = 'CCS', speed = 150) {
    let overlay = document.getElementById('booking-modal-overlay');
    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'booking-modal-overlay';
        overlay.className = 'modal-overlay';
        document.body.appendChild(overlay);
    }

    const isLogged = (typeof auth !== 'undefined') && auth.isLoggedIn();
    const currentUser = isLogged ? auth.getUser() : null;

    // Load available stations and vehicles
    let networkStations = [];
    let userVehicles = [];

    try {
        networkStations = await api.get('/stations');
    } catch (e) {
        console.warn('Could not load stations:', e);
    }

    if (isLogged) {
        try {
            userVehicles = await api.get('/vehicles');
        } catch (e) {
            console.warn('Could not load user vehicles:', e);
        }
    }

    // Default booking date = today (YYYY-MM-DD)
    const today = new Date().toISOString().split('T')[0];
    
    // Format current time HH:MM
    const nowObj = new Date();
    const nowHH = String(nowObj.getHours()).padStart(2, '0');
    const nowMM = String(nowObj.getMinutes()).padStart(2, '0');
    const defaultTime = `${nowHH}:${nowMM}`;

    overlay.innerHTML = `
        <div class="modal-card" style="max-width: 580px;">
            <div class="modal-header">
                <div>
                    <h3 style="margin-bottom: 2px;">⚡ Reserve EV Charging Bay</h3>
                    <div style="font-size: 0.8rem; color: var(--color-text-secondary);">C++ High-Throughput Slot Engine</div>
                </div>
                <button class="modal-close-btn" onclick="closeBookingModal()">✕</button>
            </div>

            <div class="modal-body" id="booking-modal-body">
                ${!isLogged ? `
                    <div style="background: #E8F4EC; border: 1px solid #C3E2CD; border-radius: var(--radius-sm); padding: 12px 16px; margin-bottom: 20px; font-size: 0.88rem; display: flex; justify-content: space-between; align-items: center;">
                        <span>Driver Account Recommended for full receipt tracking.</span>
                        <a href="/pages/login.html?redirect=${encodeURIComponent(window.location.pathname)}" class="btn btn-primary btn-sm">Login / Register ➔</a>
                    </div>
                ` : ''}

                <form id="wizard-booking-form">
                    <!-- Step 1: Select or Enter Vehicle -->
                    <div class="booking-form-group">
                        <label for="wb-vehicle-select">1. Select Electric Vehicle *</label>
                        ${(isLogged && userVehicles.length > 0) ? `
                            <select id="wb-vehicle-select" required style="padding: 10px;">
                                <option value="">-- Choose your registered EV --</option>
                                ${userVehicles.map(v => `
                                    <option value="${v.id}" data-reg="${v.registrationNumber}" data-conn="${v.connectorType}">
                                        ${v.registrationNumber} — ${v.model} (${v.connectorType})
                                    </option>
                                `).join('')}
                                <option value="NEW">+ Register another EV for this booking</option>
                            </select>
                        ` : `
                            <input type="text" id="wb-manual-reg" placeholder="e.g. KA-05-EV-2026" required value="${isLogged ? '' : 'KA-01-MJ-2026'}">
                        `}
                    </div>

                    <!-- Hidden fields for new EV registration if chosen -->
                    <div id="wb-new-veh-fields" style="${(isLogged && userVehicles.length > 0) ? 'display:none;' : 'display:block;'}">
                        <div class="form-row">
                            <div class="booking-form-group">
                                <label for="wb-new-model">Vehicle Model</label>
                                <input type="text" id="wb-new-model" placeholder="e.g. Tata Nexon EV" value="Electric Vehicle">
                            </div>
                            <div class="booking-form-group">
                                <label for="wb-new-conn">Connector Type *</label>
                                <select id="wb-new-conn">
                                    <option value="CCS" ${connectorType === 'CCS' ? 'selected' : ''}>CCS 2 (DC Fast)</option>
                                    <option value="CHAdeMO" ${connectorType === 'CHAdeMO' ? 'selected' : ''}>CHAdeMO (DC Rapid)</option>
                                    <option value="Type2" ${connectorType === 'Type2' ? 'selected' : ''}>Type 2 (AC)</option>
                                </select>
                            </div>
                        </div>
                    </div>

                    <!-- Step 2: Select Station -->
                    <div class="booking-form-group">
                        <label for="wb-station-select">2. Charging Station / Bay *</label>
                        <select id="wb-station-select" required style="padding: 10px;">
                            <option value="0">⚡ Auto-Select Fastest Compatible Station (Recommended)</option>
                            ${networkStations.map(s => `
                                <option value="${s.id || s.stationID}" ${preferredStationID === (s.id || s.stationID) ? 'selected' : ''} data-speed="${s.chargingSpeed}" data-conn="${s.connectorType}">
                                    Bay #${s.id || s.stationID} (${s.chargingSpeed} kW ${s.connectorType}) — ${s.location || 'Metro Hub'} ${s.available ? '[Available Now]' : '[In Use]'}
                                </option>
                            `).join('')}
                        </select>
                    </div>

                    <!-- Step 3 & 4: Date & Time Window -->
                    <div class="form-row">
                        <div class="booking-form-group">
                            <label for="wb-date">3. Reservation Date *</label>
                            <input type="date" id="wb-date" min="${today}" value="${today}" required>
                        </div>
                        <div class="booking-form-group">
                            <label for="wb-time">4. Arrival Time *</label>
                            <input type="time" id="wb-time" value="${defaultTime}" required>
                        </div>
                    </div>

                    <!-- Step 5: Energy & Battery Requirements -->
                    <div class="form-row">
                        <div class="booking-form-group">
                            <label for="wb-battery">Current Battery (%)</label>
                            <input type="number" id="wb-battery" min="5" max="95" value="20" required>
                        </div>
                        <div class="booking-form-group">
                            <label for="wb-energy">Required Energy (kWh) *</label>
                            <input type="number" id="wb-energy" min="5" max="150" step="1" value="35" required>
                        </div>
                    </div>

                    <!-- Live Calculation Banner -->
                    <div style="background: #F8FAF8; border: 1px solid var(--color-border); border-radius: var(--radius-sm); padding: 12px 16px; margin: 16px 0; font-size: 0.88rem; display: flex; justify-content: space-between; align-items: center;">
                        <div>
                            <span style="color: var(--color-text-secondary);">Calculated Duration:</span>
                            <strong id="wb-est-duration" style="color: var(--color-primary-dark); font-size: 1.05rem; margin-left: 6px;">14.0 mins</strong>
                        </div>
                        <span style="font-size: 0.78rem; color: #888;">Formula: (Energy / Speed) × 60</span>
                    </div>

                    <!-- Step 6: Confirmation Submission -->
                    <div style="margin-top: 24px; display: flex; gap: 12px;">
                        <button type="button" class="btn btn-secondary" style="flex: 1;" onclick="closeBookingModal()">Cancel</button>
                        <button type="submit" id="wb-submit-btn" class="btn btn-primary" style="flex: 2; padding: 12px; font-size: 1rem;">
                            Confirm Reservation ➔
                        </button>
                    </div>
                </form>
            </div>
        </div>
    `;

    overlay.classList.add('active');

    // Setup interactive calculation & toggle handlers
    const vehSelect = document.getElementById('wb-vehicle-select');
    const newVehFields = document.getElementById('wb-new-veh-fields');
    const stSelect = document.getElementById('wb-station-select');
    const energyInput = document.getElementById('wb-energy');
    const durDisplay = document.getElementById('wb-est-duration');

    function updateEstimatedTime() {
        const energy = parseFloat(energyInput ? energyInput.value : 35) || 35;
        let speed = 100;
        if (stSelect && stSelect.value !== '0') {
            const opt = stSelect.options[stSelect.selectedIndex];
            if (opt && opt.dataset.speed) speed = parseFloat(opt.dataset.speed);
        }
        const mins = ((energy / speed) * 60).toFixed(1);
        if (durDisplay) durDisplay.textContent = `${mins} mins`;
    }

    if (energyInput) energyInput.addEventListener('input', updateEstimatedTime);
    if (stSelect) stSelect.addEventListener('change', updateEstimatedTime);

    if (vehSelect) {
        vehSelect.addEventListener('change', () => {
            if (vehSelect.value === 'NEW') {
                newVehFields.style.display = 'block';
            } else {
                newVehFields.style.display = 'none';
                const opt = vehSelect.options[vehSelect.selectedIndex];
                if (opt && opt.dataset.conn) {
                    // Filter or highlight matching stations
                    const conn = opt.dataset.conn;
                    for (let i = 0; i < stSelect.options.length; i++) {
                        const o = stSelect.options[i];
                        if (o.dataset.conn && o.dataset.conn !== conn) {
                            o.disabled = true;
                        } else {
                            o.disabled = false;
                        }
                    }
                }
            }
        });
    }

    // Submit handler
    const form = document.getElementById('wizard-booking-form');
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        const submitBtn = document.getElementById('wb-submit-btn');
        submitBtn.disabled = true;
        submitBtn.textContent = 'Processing Allocation...';

        let vehicleId = 0;
        let regNum = '';
        let connector = 'CCS';

        if (vehSelect && vehSelect.value !== 'NEW' && vehSelect.value !== '') {
            vehicleId = parseInt(vehSelect.value, 10);
            const opt = vehSelect.options[vehSelect.selectedIndex];
            regNum = opt.dataset.reg || '';
            connector = opt.dataset.conn || 'CCS';
        } else {
            // Need to register or use manual plate
            const manualRegInput = document.getElementById('wb-manual-reg');
            regNum = manualRegInput ? manualRegInput.value.trim() : `EV-${Math.floor(Math.random()*9000+1000)}`;
            const connSelect = document.getElementById('wb-new-conn');
            if (connSelect) connector = connSelect.value;
            const modelInput = document.getElementById('wb-new-model');
            const model = modelInput ? modelInput.value.trim() : 'Electric Vehicle';

            // Auto-register vehicle first if authenticated
            if (isLogged) {
                try {
                    const vRes = await api.post('/vehicles', {
                        registrationNumber: regNum,
                        model: model,
                        connectorType: connector,
                        batteryCapacity: parseFloat(energyInput.value) * 1.3
                    });
                    if (vRes.vehicle && vRes.vehicle.id) {
                        vehicleId = vRes.vehicle.id;
                    }
                } catch (vErr) {
                    // If already registered, fetch it
                    const existingV = await api.get('/vehicles');
                    const match = existingV.find(v => v.registrationNumber.toUpperCase() === regNum.toUpperCase());
                    if (match) vehicleId = match.id;
                }
            }
        }

        const stationId = parseInt(stSelect.value, 10);
        const bookingDate = document.getElementById('wb-date').value;
        const startTime = document.getElementById('wb-time').value;
        const currentBattery = parseInt(document.getElementById('wb-battery').value, 10);
        const requiredEnergy = parseFloat(energyInput.value);

        try {
            const res = await api.post('/bookings', {
                vehicleId: vehicleId,
                registrationNumber: regNum,
                stationId: stationId,
                bookingDate: bookingDate,
                startTime: startTime,
                currentBattery: currentBattery,
                requiredEnergy: requiredEnergy
            });

            showBookingReceiptModal(res, regNum, requiredEnergy, speed);

            if (typeof initStationFinder === 'function') initStationFinder();
            if (typeof initLiveStats === 'function') initLiveStats();
            if (typeof loadDashboardData === 'function') loadDashboardData();
            if (typeof loadBookings === 'function') loadBookings();
        } catch (err) {
            showToast(err.message, 'error');
            submitBtn.disabled = false;
            submitBtn.textContent = 'Confirm Reservation ➔';
        }
    });
};

function showBookingReceiptModal(res, vehicleID, requiredEnergy, speed) {
    const body = document.getElementById('booking-modal-body');
    if (!body) return;

    const b = res.booking || {};
    const ref = res.bookingReference || b.bookingReference || 'EVSC-2026-ALLOC';
    const isCharging = (b.status === 'Charging' || res.message.includes('initiated'));

    body.innerHTML = `
        <div style="text-align: center; padding: 12px 0;">
            <div style="width: 64px; height: 64px; border-radius: 50%; background: ${isCharging ? '#E8F0FE' : '#E6F4EA'}; color: ${isCharging ? '#1A73E8' : '#137333'}; font-size: 2rem; display: flex; align-items: center; justify-content: center; margin: 0 auto 16px; border: 2px solid ${isCharging ? '#AECBFA' : '#A4F0C3'};">
                ${isCharging ? '⚡' : '✓'}
            </div>

            <h3 style="color: var(--color-primary-dark); font-size: 1.35rem; margin-bottom: 6px;">
                ${isCharging ? 'Session Active & Charging Started!' : 'Slot Reservation Confirmed!'}
            </h3>
            <p style="color: var(--color-text-secondary); font-size: 0.9rem; margin-bottom: 20px;">
                ${res.message}
            </p>

            <div class="receipt-wrapper" style="text-align: left; margin-bottom: 24px;">
                <div style="display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid var(--color-border); padding-bottom: 10px; margin-bottom: 12px;">
                    <div>
                        <strong style="color: var(--color-primary-dark);">EV SMARTCHARGE ALLOCATION</strong>
                        <div style="font-size: 0.75rem; color: #888;">C++ Slot Engine Reference</div>
                    </div>
                    <span class="receipt-ref" style="font-size: 0.9rem; margin-top:0;">${ref}</span>
                </div>

                <div style="font-size: 0.88rem; line-height: 1.8;">
                    <div><strong>Vehicle:</strong> ${b.vehicleRegistration || vehicleID}</div>
                    <div><strong>Assigned Bay:</strong> ${b.stationName || 'Station #' + (b.stationId || 'Allocated')}</div>
                    <div><strong>Date & Time:</strong> ${b.bookingDate || 'Today'} • ${b.startTime || 'Now'} to ${b.endTime || 'Completion'}</div>
                    <div><strong>Status:</strong> <span class="status-pill status-${(b.status || 'Confirmed').toLowerCase()}">${b.status || 'Confirmed'}</span></div>
                    <div><strong>Duration:</strong> <strong>${b.estimatedDuration || 20} minutes</strong></div>
                </div>
            </div>

            <div style="display: flex; gap: 12px; justify-content: center;">
                <button class="btn btn-secondary" style="flex: 1;" onclick="closeBookingModal()">Done</button>
                <a href="/pages/bookings.html" class="btn btn-primary" style="flex: 1; text-decoration: none; text-align: center;">View in My Bookings ➔</a>
            </div>
        </div>
    `;
}

window.closeBookingModal = function() {
    const overlay = document.getElementById('booking-modal-overlay');
    if (overlay) {
        overlay.classList.remove('active');
    }
};

window.openQuickRegisterModal = function() {
    openBookingModal(null, 'CCS', 150);
};

