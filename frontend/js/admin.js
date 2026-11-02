/**
 * EV SmartCharge - Admin Management Console JavaScript
 * Complete integration with C++ REST Backend
 */

document.addEventListener('DOMContentLoaded', () => {
    initAdminAuth();
    if (checkAdminAuth()) {
        initNavigation();
        loadDashboard();
        initForms();
        initDataStructuresExplorer();
    } else {
        initNavigation();
        initForms();
        initDataStructuresExplorer();
    }
});

// ==================== AUTHENTICATION GATE ====================
function checkAdminAuth() {
    const isAuth = (sessionStorage.getItem('ev_smartcharge_admin_auth') === 'true') || 
                   ((typeof auth !== 'undefined') && auth.isLoggedIn() && auth.isAdmin());
    const loginScreen = document.getElementById('admin-login-screen');
    const appContainer = document.getElementById('admin-app-container');

    if (isAuth) {
        if (loginScreen) loginScreen.style.display = 'none';
        if (appContainer) appContainer.style.display = 'flex';
        return true;
    } else {
        if (loginScreen) loginScreen.style.display = 'flex';
        if (appContainer) appContainer.style.display = 'none';
        return false;
    }
}

function initAdminAuth() {
    const form = document.getElementById('admin-login-form');
    const alertBox = document.getElementById('login-error-alert');

    if (form) {
        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            const user = document.getElementById('admin-user-input').value.trim();
            const pass = document.getElementById('admin-pass-input').value.trim();

            try {
                // Try database login
                const res = await api.post('/auth/login', { email: user, password: pass });
                if (res.success && res.user && res.user.role === 'admin') {
                    if (typeof auth !== 'undefined') {
                        auth.setSession(res.token, res.user, false);
                    }
                    sessionStorage.setItem('ev_smartcharge_admin_auth', 'true');
                    if (alertBox) alertBox.style.display = 'none';
                    checkAdminAuth();
                    loadDashboard();
                    showToast('Authenticated: Administrator (' + res.user.fullName + ')', 'success');
                    return;
                } else {
                    throw new Error('Access denied: Account lacks administrator privileges.');
                }
            } catch (err) {
                // Fallback for offline demo credential check
                if ((user === 'admin' || user === 'admin@smartcharge.in') && pass === 'smartcharge2026') {
                    sessionStorage.setItem('ev_smartcharge_admin_auth', 'true');
                    if (alertBox) alertBox.style.display = 'none';
                    checkAdminAuth();
                    loadDashboard();
                    showToast('Authenticated: System Administrator', 'success');
                    return;
                }

                if (alertBox) {
                    alertBox.textContent = err.message || 'Invalid administrator credentials. Access denied.';
                    alertBox.style.display = 'block';
                }
            }
        });
    }
}

window.adminLogout = function() {
    if (typeof auth !== 'undefined') auth.clearSession();
    sessionStorage.removeItem('ev_smartcharge_admin_auth');
    showToast('Signed out of Operations Console.', 'info');
    checkAdminAuth();
};

// ==================== NAVIGATION ====================
function initNavigation() {
    const navItems = document.querySelectorAll('.sidebar-nav .nav-item');
    const sections = document.querySelectorAll('.section-panel');
    const topbarTitle = document.getElementById('current-section-title');

    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const targetSection = item.getAttribute('data-section');
            if (!targetSection) return;

            navItems.forEach(n => n.classList.remove('active'));
            item.classList.add('active');

            sections.forEach(s => s.classList.remove('active'));
            const target = document.getElementById(`section-${targetSection}`);
            if (target) {
                target.classList.add('active');
            }

            if (topbarTitle) {
                const label = item.querySelector('span:last-child');
                topbarTitle.textContent = label ? label.textContent : 'Management Console';
            }

            // Refresh specific section data
            switch (targetSection) {
                case 'dashboard': loadDashboard(); break;
                case 'bookings': loadAdminBookings(); break;
                case 'users': loadAdminUsers(); break;
                case 'vehicles': loadVehicles(); break;
                case 'stations': loadStations(); break;
                case 'assignment': loadAssignmentSection(); break;
                case 'queue': loadQueue(); break;
                case 'search-sort': loadSearchSort(); break;
                case 'ds-explorer': loadDSExplorerTab('array'); break;
                case 'history': loadHistory(); break;
                case 'reports': loadReports(); break;
            }
        });
    });
}

// ==================== DASHBOARD ====================
async function loadDashboard() {
    try {
        const stats = await api.get('/dashboard');

        setText('dash-total-vehicles', stats.totalVehicles || 0);
        setText('dash-total-stations', stats.totalStations || 0);
        setText('dash-available-slots', stats.availableSlots || 0);
        setText('dash-occupied-slots', stats.occupiedSlots || 0);
        setText('dash-waiting-vehicles', stats.waitingVehicles || 0);
        setText('dash-total-assignments', stats.totalAssignments || 0);
        setText('dash-utilization', `${stats.stationUtilization ? stats.stationUtilization.toFixed(1) : 0}%`);

        // Load mini stations status
        const stations = await api.get('/stations');
        const stationsGrid = document.getElementById('dash-stations-overview');
        if (stationsGrid) {
            stationsGrid.innerHTML = stations.map(s => `
                <div style="background:#fff; border:1px solid var(--color-border); border-radius:var(--radius-sm); padding:12px; display:flex; justify-content:space-between; align-items:center;">
                    <div>
                        <div style="font-weight:700; color:var(--color-primary-dark);">Station #${s.stationID}</div>
                        <div style="font-size:0.8rem; color:var(--color-text-muted);">${s.chargingSpeed} kW • ${s.connectorType}</div>
                    </div>
                    <div>
                        <span class="badge ${s.available ? 'badge-available' : 'badge-occupied'}">${s.available ? 'Available' : 'Occupied'}</span>
                    </div>
                </div>
            `).join('');
        }

        // Load recent history
        const history = await api.get('/history');
        const historyTbody = document.getElementById('dash-recent-history-tbody');
        if (historyTbody) {
            const recent = history.slice(0, 5);
            if (recent.length === 0) {
                historyTbody.innerHTML = '<tr><td colspan="4" style="text-align:center; color:var(--color-text-muted);">No recorded actions yet.</td></tr>';
            } else {
                historyTbody.innerHTML = recent.map(h => `
                    <tr>
                        <td><span style="font-size:0.8rem; font-family:monospace;">${h.timestamp}</span></td>
                        <td><span class="badge ${h.actionType === 'ASSIGNMENT' ? 'badge-available' : 'badge-type'}">${h.actionType}</span></td>
                        <td><strong>${h.vehicleID}</strong></td>
                        <td>Station #${h.stationID}</td>
                    </tr>
                `).join('');
            }
        }
    } catch (err) {
        console.error('Error loading dashboard:', err);
    }
}

// ==================== VEHICLES ====================
async function loadVehicles() {
    const tbody = document.getElementById('vehicles-table-tbody');
    if (!tbody) return;

    try {
        tbody.innerHTML = '<tr><td colspan="7" style="text-align:center; padding:20px;">Loading vehicles from C++ backend...</td></tr>';
        const vehicles = await api.get('/vehicles');

        if (vehicles.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align:center; padding:20px; color:var(--color-text-muted);">No vehicles registered in system.</td></tr>';
            return;
        }

        tbody.innerHTML = vehicles.map(v => `
            <tr>
                <td><strong>${v.vehicleID}</strong></td>
                <td>
                    <div style="display:flex; align-items:center; gap:8px;">
                        <div style="width:50px; background:#ECEFF1; height:8px; border-radius:4px; overflow:hidden;">
                            <div style="width:${v.batteryLevel}%; background:${v.batteryLevel < 25 ? 'var(--color-error)' : 'var(--color-primary)'}; height:100%;"></div>
                        </div>
                        <span>${v.batteryLevel}%</span>
                    </div>
                </td>
                <td>${v.requiredEnergy} kWh</td>
                <td><span class="badge badge-type">${v.connectorType}</span></td>
                <td>${v.ownerName || '<span style="color:#aaa;">—</span>'}</td>
                <td><span class="badge badge-queued">${v.membershipCategory || 'Regular'}</span></td>
                <td>
                    <button class="btn btn-danger btn-sm" onclick="deleteVehicle('${v.vehicleID}')">Delete</button>
                </td>
            </tr>
        `).join('');
    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="7" style="color:var(--color-error); text-align:center;">Error: ${err.message}</td></tr>`;
    }
}

window.deleteVehicle = async function(vehicleID) {
    if (!confirm(`Are you sure you want to delete vehicle ${vehicleID}?`)) return;
    try {
        const res = await api.delete(`/vehicles/${vehicleID}`);
        showToast(res.message || 'Vehicle deleted', 'success');
        loadVehicles();
        loadDashboard();
    } catch (err) {
        showToast(err.message, 'error');
    }
};

// ==================== STATIONS ====================
async function loadStations() {
    const grid = document.getElementById('stations-management-grid');
    if (!grid) return;

    try {
        grid.innerHTML = '<div style="grid-column:1/-1; text-align:center; padding:20px;">Loading stations from C++ backend...</div>';
        const stations = await api.get('/stations');

        grid.innerHTML = stations.map(s => `
            <div class="station-card">
                <div class="station-card-header">
                    <div class="station-id">⚡ Station #${s.stationID}</div>
                    <span class="badge ${s.available ? 'badge-available' : 'badge-occupied'}">${s.available ? 'Available' : 'Occupied'}</span>
                </div>
                <div class="station-specs">
                    <div><strong>Speed:</strong> ${s.chargingSpeed} kW (DC Fast)</div>
                    <div><strong>Connector:</strong> <span class="badge badge-type">${s.connectorType}</span></div>
                    <div><strong>Location:</strong> ${s.location || 'Metro Expressway Corridor Hub'}</div>
                    ${!s.available ? `<div><strong>Charging:</strong> <span style="color:var(--color-primary-dark); font-weight:700;">${s.currentVehicle}</span></div>` : ''}
                </div>
                <div style="margin-top:auto; padding-top:12px;">
                    ${!s.available ? `
                        <button class="btn btn-primary btn-sm" style="width:100%;" onclick="completeCharging(${s.stationID})">
                            ✓ Complete Charging
                        </button>
                    ` : `
                        <div style="font-size:0.8rem; text-align:center; color:var(--color-primary); font-weight:600;">Ready for Assignment</div>
                    `}
                </div>
            </div>
        `).join('');
    } catch (err) {
        grid.innerHTML = `<div style="grid-column:1/-1; color:var(--color-error); text-align:center;">Error: ${err.message}</div>`;
    }
}

window.completeCharging = async function(stationID) {
    try {
        const res = await api.post('/charging/complete', { stationID });
        showToast(res.message || `Charging completed for Station #${stationID}`, 'success');
        loadStations();
        loadDashboard();
        loadQueue();
    } catch (err) {
        showToast(err.message, 'error');
    }
};

// ==================== SLOT ASSIGNMENT ====================
async function loadAssignmentSection() {
    const select = document.getElementById('assign-vehicle-select');
    if (!select) return;

    try {
        const vehicles = await api.get('/vehicles');
        select.innerHTML = '<option value="">-- Choose a Registered Vehicle --</option>' +
            vehicles.map(v => `<option value="${v.vehicleID}">${v.vehicleID} (${v.connectorType}, ${v.requiredEnergy} kWh needed, Battery ${v.batteryLevel}%)</option>`).join('');
    } catch (err) {
        console.error('Error populating vehicle select:', err);
    }
}

// ==================== WAITING QUEUE ====================
async function loadQueue() {
    const tbody = document.getElementById('queue-table-tbody');
    const queueCountBadge = document.getElementById('queue-count-badge');
    if (!tbody) return;

    try {
        tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; padding:20px;">Loading waiting queue from C++ backend...</td></tr>';
        const queueData = await api.get('/queue');
        const queue = queueData.items || [];

        if (queueCountBadge) queueCountBadge.textContent = `${queue.length} Waiting`;

        if (queue.length === 0) {
            tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; padding:24px; color:var(--color-text-muted);">The waiting queue is currently empty. All compatible vehicles have slots.</td></tr>';
            return;
        }

        tbody.innerHTML = queue.map((v, idx) => `
            <tr>
                <td><strong>#${idx + 1}</strong></td>
                <td><strong>${v.vehicleID}</strong></td>
                <td>${v.batteryLevel}%</td>
                <td>${v.requiredEnergy} kWh</td>
                <td><span class="badge badge-type">${v.connectorType}</span></td>
                <td><span class="badge badge-queued">Waiting for Compatible Slot</span></td>
            </tr>
        `).join('');
    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="6" style="color:var(--color-error); text-align:center;">Error: ${err.message}</td></tr>`;
    }
}

window.processWaitingQueue = async function() {
    try {
        const res = await api.post('/queue/process', {});
        showToast(res.message || 'Waiting queue processed.', 'success');
        loadQueue();
        loadStations();
        loadDashboard();
    } catch (err) {
        showToast(err.message, 'error');
    }
};

// ==================== SEARCH & SORT ====================
async function loadSearchSort() {
    // Initial display
}

window.runSearchDemo = async function() {
    const type = document.getElementById('search-type').value;
    const query = document.getElementById('search-query').value.trim();
    const resultBox = document.getElementById('search-results-box');

    if (!query) {
        showToast('Please enter a search query.', 'warning');
        return;
    }

    try {
        resultBox.innerHTML = 'Searching in C++ backend...';
        const res = await api.post('/datastructures/search', { type, query });
        
        let html = `
            <div style="background:#fff; border:1px solid var(--color-border); border-radius:var(--radius-sm); padding:16px;">
                <div style="display:flex; justify-content:space-between; margin-bottom:12px;">
                    <strong>Algorithm: ${res.algorithm || 'Search'}</strong>
                    <span class="badge badge-type">Time: ${res.timeComplexity || 'O(N)'}</span>
                </div>
                <p><strong>Query:</strong> "${query}"</p>
                <p><strong>Status:</strong> ${res.found ? '✓ FOUND' : '✗ NOT FOUND'}</p>
                <p><strong>Comparisons / Steps:</strong> ${res.comparisons || 1}</p>
                ${res.result ? `<pre style="background:#F9FAF9; padding:10px; border-radius:4px; margin-top:10px; font-size:0.85rem;">${JSON.stringify(res.result, null, 2)}</pre>` : ''}
            </div>
        `;
        resultBox.innerHTML = html;
    } catch (err) {
        resultBox.innerHTML = `<div style="color:var(--color-error);">Error: ${err.message}</div>`;
    }
};

window.runSortDemo = async function() {
    const field = document.getElementById('sort-field').value;
    const order = document.getElementById('sort-order').value;
    const resultBox = document.getElementById('sort-results-box');

    try {
        resultBox.innerHTML = 'Executing Bubble Sort in C++ backend...';
        const res = await api.post('/datastructures/sort', { field, order });

        let stepsHtml = '';
        if (res.steps && res.steps.length > 0) {
            stepsHtml = res.steps.map(s => `
                <div class="step-row">Pass ${s.pass}, Comp ${s.comparison}: ${s.description}</div>
            `).join('');
        } else {
            stepsHtml = '<div class="step-row">Already sorted — 0 swaps needed.</div>';
        }

        let html = `
            <div style="background:#fff; border:1px solid var(--color-border); border-radius:var(--radius-sm); padding:16px;">
                <div style="display:flex; justify-content:space-between; margin-bottom:12px;">
                    <strong>Algorithm: ${res.algorithm}</strong>
                    <span class="badge badge-type">Worst: O(N²) • Best: O(N)</span>
                </div>
                <div style="display:grid; grid-template-columns:1fr 1fr; gap:16px; margin-bottom:16px;">
                    <div><strong>Total Comparisons:</strong> ${res.totalComparisons}</div>
                    <div><strong>Total Swaps:</strong> ${res.totalSwaps}</div>
                </div>
                <div style="margin-bottom:8px; font-weight:600; font-size:0.85rem;">Step-by-Step Swaps:</div>
                <div class="steps-container">${stepsHtml}</div>
            </div>
        `;
        resultBox.innerHTML = html;
        loadStations(); // Refresh station list since sort reorders
    } catch (err) {
        resultBox.innerHTML = `<div style="color:var(--color-error);">Error: ${err.message}</div>`;
    }
};

// ==================== DATA STRUCTURES EXPLORER ====================
function initDataStructuresExplorer() {
    const tabBtns = document.querySelectorAll('.ds-tab-btn');
    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const tabName = btn.getAttribute('data-tab');
            loadDSExplorerTab(tabName);
        });
    });
}

window.loadDSExplorerTab = async function(tabName) {
    document.querySelectorAll('.ds-tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.ds-tab-content').forEach(c => c.classList.remove('active'));

    const btn = document.querySelector(`.ds-tab-btn[data-tab="${tabName}"]`);
    const content = document.getElementById(`ds-tab-${tabName}`);
    if (btn) btn.classList.add('active');
    if (content) content.classList.add('active');

    switch (tabName) {
        case 'array': renderArrayDemo(); break;
        case 'stack': renderStackDemo(); break;
        case 'queue': renderQueueDemo(); break;
        case 'linkedlist': renderLinkedListDemo(); break;
        case 'searching': renderSearchingDemo(); break;
        case 'sorting': renderSortingDemo(); break;
    }
};

// Array Demo
async function renderArrayDemo() {
    const container = document.getElementById('ds-array-visual');
    if (!container) return;
    try {
        const vehicles = await api.get('/vehicles');
        container.innerHTML = `
            <div class="array-visual-wrapper">
                ${vehicles.map((v, i) => `
                    <div class="array-cell">
                        <span class="array-cell-index">[${i}]</span>
                        <div class="array-cell-value">${v.vehicleID}<br><span style="font-size:0.75rem; color:#666;">${v.connectorType}</span></div>
                    </div>
                `).join('')}
            </div>
        `;
    } catch (err) {
        container.innerHTML = `<div style="color:var(--color-error);">Error: ${err.message}</div>`;
    }
}

// Stack Demo
async function renderStackDemo() {
    const container = document.getElementById('ds-stack-visual');
    const infoBox = document.getElementById('ds-stack-info');
    if (!container) return;

    try {
        const state = await api.get('/datastructures/stack');
        const items = state.items || [];

        if (infoBox) {
            infoBox.innerHTML = `Stack Size: <strong>${state.size}</strong> / Capacity: <strong>${state.capacity}</strong> | Is Empty: <strong>${state.isEmpty}</strong> | Is Full: <strong>${state.isFull}</strong>`;
        }

        if (items.length === 0) {
            container.innerHTML = '<div style="color:var(--color-text-muted); font-size:0.85rem;">Stack is currently empty. Push an action!</div>';
            return;
        }

        container.innerHTML = `
            <div class="stack-visual-wrapper">
                ${items.map((item, idx) => `
                    <div class="stack-item-box ${idx === items.length - 1 ? 'is-top' : ''}">
                        ${idx === items.length - 1 ? '⭐ [TOP] ' : ''}#${idx} ${item.actionType}: ${item.vehicleID}
                    </div>
                `).join('')}
            </div>
        `;
    } catch (err) {
        container.innerHTML = `<div style="color:var(--color-error);">Error: ${err.message}</div>`;
    }
}

window.pushDemoStack = async function() {
    const action = prompt('Enter Action Type (e.g. MANUAL_PUSH, REFUEL_TEST, CHARGING_START):', 'MANUAL_PUSH');
    if (!action) return;
    const vehicle = prompt('Enter Vehicle ID:', 'DEMO-9999');
    if (!vehicle) return;

    try {
        await api.post('/datastructures/stack/push', {
            actionType: action,
            vehicleID: vehicle,
            stationID: 101,
            timestamp: new Date().toLocaleTimeString(),
            details: 'Manual test push to C++ ActionStack'
        });
        showToast('Pushed element onto C++ Stack (LIFO)', 'success');
        renderStackDemo();
    } catch (err) {
        showToast(err.message, 'error');
    }
};

window.popDemoStack = async function() {
    try {
        const res = await api.post('/datastructures/stack/pop', {});
        if (res.success) {
            showToast(`Popped from C++ Stack: ${res.popped ? res.popped.actionType : 'item'}`, 'success');
        } else {
            showToast(res.message || 'Stack underflow!', 'warning');
        }
        renderStackDemo();
    } catch (err) {
        showToast(err.message, 'error');
    }
};

// Queue Demo
async function renderQueueDemo() {
    const container = document.getElementById('ds-queue-visual');
    const infoBox = document.getElementById('ds-queue-info');
    if (!container) return;

    try {
        const state = await api.get('/datastructures/queue');
        const items = state.items || [];

        if (infoBox) {
            infoBox.innerHTML = `Queue Size: <strong>${state.size}</strong> / Capacity: <strong>${state.capacity}</strong> | Front Index: <strong>${state.frontIndex}</strong> | Rear Index: <strong>${state.rearIndex}</strong>`;
        }

        if (items.length === 0) {
            container.innerHTML = '<div style="color:var(--color-text-muted); font-size:0.85rem;">Queue is empty. All vehicles assigned!</div>';
            return;
        }

        container.innerHTML = `
            <div class="queue-visual-wrapper">
                <span style="font-weight:700; color:var(--color-primary); font-size:0.85rem;">[FRONT] ➔</span>
                ${items.map((item, idx) => `
                    <div class="queue-node">
                        <strong>${item.vehicleID}</strong><br>
                        <span style="font-size:0.75rem;">${item.connectorType}</span>
                    </div>
                    ${idx < items.length - 1 ? '<span class="queue-arrow">➔</span>' : ''}
                `).join('')}
                <span style="font-weight:700; color:var(--color-text-muted); font-size:0.85rem;">➔ [REAR]</span>
            </div>
        `;
    } catch (err) {
        container.innerHTML = `<div style="color:var(--color-error);">Error: ${err.message}</div>`;
    }
}

// Linked List Demo
async function renderLinkedListDemo() {
    const container = document.getElementById('ds-linkedlist-visual');
    const infoBox = document.getElementById('ds-linkedlist-info');
    if (!container) return;

    try {
        const state = await api.get('/datastructures/linkedlist');
        const nodes = state.nodes || [];

        if (infoBox) {
            infoBox.innerHTML = `Total Nodes: <strong>${state.size}</strong> | Dynamic Memory Allocation (Heap Pointers) | Is Empty: <strong>${state.isEmpty}</strong>`;
        }

        if (nodes.length === 0) {
            container.innerHTML = '<div style="color:var(--color-text-muted); font-size:0.85rem;">Linked list is currently empty.</div>';
            return;
        }

        container.innerHTML = `
            <div class="linkedlist-visual-wrapper">
                <span style="font-weight:700; color:var(--color-primary); font-size:0.85rem;">HEAD ➔</span>
                ${nodes.map(n => `
                    <div class="ll-node">
                        <div class="ll-node-data">
                            <div><strong>${n.data.vehicleID}</strong></div>
                            <div style="font-size:0.7rem; color:var(--color-text-muted);">${n.data.action}</div>
                        </div>
                        <div class="ll-node-next">NEXT</div>
                    </div>
                    <span class="ll-arrow">➔</span>
                `).join('')}
                <div class="ll-null">NULL</div>
            </div>
        `;
    } catch (err) {
        container.innerHTML = `<div style="color:var(--color-error);">Error: ${err.message}</div>`;
    }
}

window.insertDemoLinkedList = async function() {
    const vid = prompt('Enter Vehicle ID for new Linked List node:', 'LL-TEST-01');
    if (!vid) return;

    try {
        await api.post('/datastructures/linkedlist/insert', {
            requestID: 'REQ-' + Date.now().toString().slice(-4),
            vehicleID: vid,
            action: 'SESSION_NODE',
            timestamp: new Date().toLocaleTimeString(),
            details: 'Node inserted dynamically into C++ singly linked list'
        });
        showToast('Node inserted dynamically at HEAD in C++ Linked List', 'success');
        renderLinkedListDemo();
    } catch (err) {
        showToast(err.message, 'error');
    }
};

function renderSearchingDemo() {
    // Already handled in Search & Sort tab
}

function renderSortingDemo() {
    // Already handled in Search & Sort tab
}

// ==================== ASSIGNMENT HISTORY ====================
async function loadHistory() {
    const tbody = document.getElementById('history-table-tbody');
    if (!tbody) return;

    try {
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; padding:20px;">Loading history from C++ stack...</td></tr>';
        const history = await api.get('/history');

        if (history.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; padding:20px; color:var(--color-text-muted);">No action history recorded yet.</td></tr>';
            return;
        }

        tbody.innerHTML = history.map(h => `
            <tr>
                <td><span style="font-family:monospace; font-size:0.85rem;">${h.timestamp}</span></td>
                <td><span class="badge ${h.actionType === 'ASSIGNMENT' ? 'badge-available' : 'badge-type'}">${h.actionType}</span></td>
                <td><strong>${h.vehicleID}</strong></td>
                <td>Station #${h.stationID}</td>
                <td><span style="font-size:0.85rem; color:var(--color-text-muted);">${h.details}</span></td>
            </tr>
        `).join('');
    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="5" style="color:var(--color-error); text-align:center;">Error: ${err.message}</td></tr>`;
    }
}

// ==================== REPORTS & ANALYTICS ====================
async function loadReports() {
    try {
        const stats = await api.get('/dashboard');
        const stations = await api.get('/stations');
        const vehicles = await api.get('/vehicles');

        setText('rep-total-vehicles', stats.totalVehicles || 0);
        setText('rep-total-stations', stats.totalStations || 0);
        setText('rep-utilization-val', `${stats.stationUtilization ? stats.stationUtilization.toFixed(1) : 0}%`);

        // Connector distribution
        const connectors = {};
        stations.forEach(s => {
            connectors[s.connectorType] = (connectors[s.connectorType] || 0) + 1;
        });

        const connectorBox = document.getElementById('rep-connector-distribution');
        if (connectorBox) {
            connectorBox.innerHTML = Object.entries(connectors).map(([type, count]) => `
                <div style="margin-bottom:10px;">
                    <div style="display:flex; justify-content:space-between; font-size:0.85rem; margin-bottom:4px;">
                        <strong>${type}</strong>
                        <span>${count} station(s) (${((count / stations.length) * 100).toFixed(0)}%)</span>
                    </div>
                    <div style="background:#ECEFF1; height:8px; border-radius:4px; overflow:hidden;">
                        <div style="width:${(count / stations.length) * 100}%; background:var(--color-primary); height:100%;"></div>
                    </div>
                </div>
            `).join('');
        }
    } catch (err) {
        console.error('Error loading reports:', err);
    }
}

// ==================== FORM SUBMISSIONS ====================
function initForms() {
    // Vehicle Registration Form
    const vehicleForm = document.getElementById('vehicle-register-form');
    if (vehicleForm) {
        vehicleForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const vehicleID = document.getElementById('veh-id').value.trim();
            const batteryLevel = parseInt(document.getElementById('veh-battery').value, 10);
            const requiredEnergy = parseFloat(document.getElementById('veh-energy').value);
            const connectorType = document.getElementById('veh-connector').value;
            const ownerName = document.getElementById('veh-owner').value.trim();

            if (!vehicleID || isNaN(batteryLevel) || isNaN(requiredEnergy) || !connectorType) {
                showToast('Please fill in all required fields.', 'warning');
                return;
            }

            try {
                const res = await api.post('/vehicles', {
                    vehicleID,
                    batteryLevel,
                    requiredEnergy,
                    connectorType,
                    ownerName
                });

                showToast(res.message || 'Vehicle registered successfully!', 'success');
                vehicleForm.reset();
                loadVehicles();
                loadDashboard();
                loadQueue();
            } catch (err) {
                showToast(err.message, 'error');
            }
        });
    }

    // Station Registration Form
    const stationForm = document.getElementById('station-register-form');
    if (stationForm) {
        stationForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const stationID = parseInt(document.getElementById('st-id').value, 10);
            const chargingSpeed = parseFloat(document.getElementById('st-speed').value);
            const connectorType = document.getElementById('st-connector').value;
            const location = document.getElementById('st-location').value.trim();

            if (isNaN(stationID) || isNaN(chargingSpeed) || !connectorType) {
                showToast('Please fill in all required station fields.', 'warning');
                return;
            }

            try {
                const res = await api.post('/stations', {
                    stationID,
                    chargingSpeed,
                    connectorType,
                    location: location || 'Metro Expressway Corridor Hub Bay',
                    available: true,
                    currentVehicle: ""
                });

                showToast(res.message || 'Station registered successfully!', 'success');
                stationForm.reset();
                loadStations();
                loadDashboard();
            } catch (err) {
                showToast(err.message, 'error');
            }
        });
    }

    // Slot Assignment Form
    const assignForm = document.getElementById('manual-assign-form');
    if (assignForm) {
        assignForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const vehicleID = document.getElementById('assign-vehicle-select').value;
            if (!vehicleID) {
                showToast('Please select a vehicle to assign.', 'warning');
                return;
            }

            try {
                const res = await api.post('/api/assign', { vehicleID });
                showToast(res.message || 'Assignment operation executed.', res.assigned ? 'success' : 'info');
                loadStations();
                loadQueue();
                loadDashboard();
            } catch (err) {
                showToast(err.message, 'error');
            }
        });
    }
}

// ==================== ADMIN BOOKINGS ====================
let allAdminBookings = [];

async function loadAdminBookings() {
    const tbody = document.getElementById('admin-bookings-tbody');
    if (!tbody) return;

    try {
        tbody.innerHTML = '<tr><td colspan="8" style="text-align:center; padding:24px;">Loading reservations from database...</td></tr>';

        const token = auth.getToken();
        const headers = token ? { 'Authorization': `Bearer ${token}` } : {};
        const res = await fetch('/api/admin/bookings', { headers });
        const data = await res.json();

        allAdminBookings = data.bookings || data || [];

        renderAdminBookingsTable(allAdminBookings);
    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="8" style="color:var(--color-error); text-align:center; padding:24px;">Error: ${err.message}</td></tr>`;
    }
}

function renderAdminBookingsTable(bookings) {
    const tbody = document.getElementById('admin-bookings-tbody');
    if (!tbody) return;

    if (bookings.length === 0) {
        tbody.innerHTML = '<tr><td colspan="8" style="text-align:center; padding:30px; color:#999;">No reservations found matching filters.</td></tr>';
        return;
    }

    tbody.innerHTML = bookings.map(b => {
        const statusClass = getStatusClass(b.status);
        const canProgress = (b.status === 'Confirmed' || b.status === 'Waiting');
        const canComplete = (b.status === 'Charging');
        const canCancel = (b.status === 'Confirmed' || b.status === 'Waiting');

        return `
            <tr data-booking-id="${b.id}">
                <td><strong style="font-family:monospace; font-size:0.85rem;">${b.bookingReference || b.booking_reference || '—'}</strong></td>
                <td>${b.customerName || b.customer_name || '—'}<br><span style="font-size:0.75rem; color:var(--color-text-muted);">${b.customerEmail || b.customer_email || ''}</span></td>
                <td>${b.vehicleRegistration || b.vehicle_registration || b.vehicleId || '—'}<br><span style="font-size:0.75rem; color:var(--color-text-muted);">${b.connectorType || b.connector_type || ''}</span></td>
                <td>Station #${b.stationId || b.station_id || '—'}</td>
                <td>${b.bookingDate || b.booking_date || '—'}<br><span style="font-size:0.75rem; color:var(--color-text-muted);">${b.startTime || b.start_time || ''} – ${b.endTime || b.end_time || ''}</span></td>
                <td>${b.duration || b.estimatedDuration || '—'} min</td>
                <td><span class="badge ${statusClass}">${b.status}</span></td>
                <td>
                    ${canProgress ? `<button class="btn btn-primary btn-sm" onclick="updateBookingStatus(${b.id}, 'Charging')" style="margin:2px; font-size:0.75rem;">▶ Start</button>` : ''}
                    ${canComplete ? `<button class="btn btn-primary btn-sm" onclick="updateBookingStatus(${b.id}, 'Completed')" style="margin:2px; font-size:0.75rem;">✓ Complete</button>` : ''}
                    ${canCancel ? `<button class="btn btn-danger btn-sm" onclick="updateBookingStatus(${b.id}, 'Cancelled')" style="margin:2px; font-size:0.75rem;">✗ Cancel</button>` : ''}
                    ${b.status === 'Completed' || b.status === 'Cancelled' ? '<span style="font-size:0.75rem; color:#999;">No actions</span>' : ''}
                </td>
            </tr>
        `;
    }).join('');
}

function getStatusClass(status) {
    switch ((status || '').toLowerCase()) {
        case 'confirmed': return 'badge-available';
        case 'charging': return 'badge-occupied';
        case 'completed': return 'badge-type';
        case 'waiting': return 'badge-queued';
        case 'cancelled': return 'badge-cancelled';
        default: return 'badge-type';
    }
}

window.updateBookingStatus = async function(bookingId, newStatus) {
    const action = newStatus === 'Cancelled' ? 'cancel' : `set to "${newStatus}"`;
    if (!confirm(`Are you sure you want to ${action} this booking?`)) return;

    try {
        const token = auth.getToken();
        const headers = {
            'Content-Type': 'application/json',
            ...(token ? { 'Authorization': `Bearer ${token}` } : {})
        };

        const res = await fetch(`/api/admin/bookings/${bookingId}/status`, {
            method: 'PUT',
            headers,
            body: JSON.stringify({ status: newStatus })
        });
        const data = await res.json();

        if (!res.ok) throw new Error(data.message || 'Failed to update booking');

        showToast(data.message || `Booking updated to ${newStatus}`, 'success');
        loadAdminBookings();
        loadDashboard();
        loadStations();
    } catch (err) {
        showToast(err.message, 'error');
    }
};

window.filterAdminBookings = function() {
    const searchText = (document.getElementById('admin-book-search')?.value || '').toLowerCase();
    const statusFilter = document.getElementById('admin-book-status')?.value || 'ALL';
    const dateFilter = document.getElementById('admin-book-date')?.value || '';

    let filtered = allAdminBookings;

    if (searchText) {
        filtered = filtered.filter(b => {
            const ref = (b.bookingReference || b.booking_reference || '').toLowerCase();
            const vehicle = (b.vehicleRegistration || b.vehicle_registration || b.vehicleId || '').toLowerCase();
            const customer = (b.customerName || b.customer_name || '').toLowerCase();
            const email = (b.customerEmail || b.customer_email || '').toLowerCase();
            return ref.includes(searchText) || vehicle.includes(searchText) || customer.includes(searchText) || email.includes(searchText);
        });
    }

    if (statusFilter !== 'ALL') {
        filtered = filtered.filter(b => b.status === statusFilter);
    }

    if (dateFilter) {
        filtered = filtered.filter(b => (b.bookingDate || b.booking_date || '') === dateFilter);
    }

    renderAdminBookingsTable(filtered);
};

// ==================== ADMIN USERS ====================
let allAdminUsers = [];

async function loadAdminUsers() {
    const tbody = document.getElementById('admin-users-tbody');
    if (!tbody) return;

    try {
        tbody.innerHTML = '<tr><td colspan="7" style="text-align:center; padding:24px;">Loading customer accounts...</td></tr>';

        const token = auth.getToken();
        const headers = token ? { 'Authorization': `Bearer ${token}` } : {};
        const res = await fetch('/api/admin/users', { headers });
        const data = await res.json();

        allAdminUsers = data.users || data || [];

        renderAdminUsersTable(allAdminUsers);
    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="7" style="color:var(--color-error); text-align:center; padding:24px;">Error: ${err.message}</td></tr>`;
    }
}

function renderAdminUsersTable(users) {
    const tbody = document.getElementById('admin-users-tbody');
    if (!tbody) return;

    if (users.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" style="text-align:center; padding:30px; color:#999;">No customer accounts found.</td></tr>';
        return;
    }

    tbody.innerHTML = users.map(u => `
        <tr>
            <td><strong>#${u.id}</strong></td>
            <td>${u.fullName || u.full_name || '—'}</td>
            <td><span style="font-size:0.85rem;">${u.email || '—'}</span></td>
            <td>${u.mobile || '—'}</td>
            <td><span class="badge ${u.role === 'admin' ? 'badge-occupied' : 'badge-available'}">${u.role || 'customer'}</span></td>
            <td><span style="font-size:0.8rem; color:var(--color-text-muted);">${u.createdAt || u.created_at || '—'}</span></td>
            <td><span class="badge badge-available">Active</span></td>
        </tr>
    `).join('');
}

window.filterAdminUsers = function() {
    const searchText = (document.getElementById('admin-user-search')?.value || '').toLowerCase();

    if (!searchText) {
        renderAdminUsersTable(allAdminUsers);
        return;
    }

    const filtered = allAdminUsers.filter(u => {
        const name = (u.fullName || u.full_name || '').toLowerCase();
        const email = (u.email || '').toLowerCase();
        const mobile = (u.mobile || '').toLowerCase();
        return name.includes(searchText) || email.includes(searchText) || mobile.includes(searchText);
    });

    renderAdminUsersTable(filtered);
};

// ==================== UTILITY ====================
function setText(id, text) {
    const el = document.getElementById(id);
    if (el) el.textContent = text;
}

