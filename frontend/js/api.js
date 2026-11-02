/**
 * EV SmartCharge - API Client & Session Manager
 * Connects frontend with C++ REST API and SQLite Database
 */

const API_BASE = '/api';

const api = {
    async request(endpoint, options = {}) {
        const url = `${API_BASE}${endpoint}`;
        const headers = {
            'Content-Type': 'application/json',
            'Accept': 'application/json',
            ...(options.headers || {})
        };

        // Attach Authorization token if available
        const token = auth.getToken();
        if (token) {
            headers['Authorization'] = `Bearer ${token}`;
        }

        const config = {
            ...options,
            headers
        };

        try {
            const response = await fetch(url, config);
            const contentType = response.headers.get('content-type') || '';
            let data = null;

            if (contentType.includes('application/json')) {
                data = await response.json();
            } else {
                data = await response.text();
            }

            if (!response.ok) {
                const message = (data && data.message) ? data.message : `HTTP Error ${response.status}`;
                throw new Error(message);
            }

            return data;
        } catch (error) {
            console.error(`[API Error] ${options.method || 'GET'} ${endpoint}:`, error);
            throw error;
        }
    },

    get(endpoint) {
        return this.request(endpoint, { method: 'GET' });
    },

    post(endpoint, body) {
        return this.request(endpoint, {
            method: 'POST',
            body: JSON.stringify(body)
        });
    },

    put(endpoint, body) {
        return this.request(endpoint, {
            method: 'PUT',
            body: JSON.stringify(body)
        });
    },

    delete(endpoint) {
        return this.request(endpoint, { method: 'DELETE' });
    }
};

// ==================== AUTHENTICATION HELPER ====================
const auth = {
    getToken() {
        return localStorage.getItem('evsc_token') || sessionStorage.getItem('evsc_token') || '';
    },

    getUser() {
        try {
            const raw = localStorage.getItem('evsc_user') || sessionStorage.getItem('evsc_user');
            return raw ? JSON.parse(raw) : null;
        } catch (e) {
            return null;
        }
    },

    isLoggedIn() {
        return !!this.getToken();
    },

    isAdmin() {
        const u = this.getUser();
        return u && u.role === 'admin';
    },

    setSession(token, user, remember = false) {
        const storage = remember ? localStorage : sessionStorage;
        storage.setItem('evsc_token', token);
        storage.setItem('evsc_user', JSON.stringify(user));
        if (user.role === 'admin') {
            sessionStorage.setItem('ev_smartcharge_admin_auth', 'true');
        }

        // Clean alternate storage
        if (remember) {
            sessionStorage.removeItem('evsc_token');
            sessionStorage.removeItem('evsc_user');
        } else {
            localStorage.removeItem('evsc_token');
            localStorage.removeItem('evsc_user');
        }
    },

    clearSession() {
        localStorage.removeItem('evsc_token');
        localStorage.removeItem('evsc_user');
        sessionStorage.removeItem('evsc_token');
        sessionStorage.removeItem('evsc_user');
        sessionStorage.removeItem('ev_smartcharge_admin_auth');
    },

    async logout() {
        try {
            await api.post('/auth/logout', {});
        } catch (e) {
            // Ignore network errors on logout
        }
        this.clearSession();
        window.location.href = '/pages/login.html';
    },

    requireAuth() {
        if (!this.isLoggedIn()) {
            const curPath = encodeURIComponent(window.location.pathname);
            window.location.href = `/pages/login.html?redirect=${curPath}`;
            return false;
        }
        return true;
    },

    requireAdmin() {
        if (!this.isLoggedIn() || !this.isAdmin()) {
            window.location.href = '/admin.html';
            return false;
        }
        return true;
    }
};

// Global navbar sync for authenticated users
function syncNavbarAuth() {
    const navList = document.querySelector('.nav-list');
    if (!navList) return;

    const user = auth.getUser();
    const isLogged = auth.isLoggedIn();

    // Remove any previous auth-specific buttons
    navList.querySelectorAll('.nav-auth-item').forEach(el => el.remove());

    if (isLogged && user) {
        const firstName = user.fullName ? user.fullName.split(' ')[0] : 'Account';
        const dashboardUrl = (user.role === 'admin') ? '/admin.html' : '/pages/dashboard.html';

        const myBookingsLi = document.createElement('li');
        myBookingsLi.className = 'nav-auth-item';
        myBookingsLi.innerHTML = `<a href="/pages/bookings.html">My Bookings</a>`;

        const dashboardLi = document.createElement('li');
        dashboardLi.className = 'nav-auth-item';
        dashboardLi.innerHTML = `<a href="${dashboardUrl}" style="font-weight:600; color:var(--color-primary-dark);">👤 ${firstName}</a>`;

        const logoutLi = document.createElement('li');
        logoutLi.className = 'nav-auth-item';
        logoutLi.innerHTML = `<a href="#" onclick="auth.logout(); return false;" style="color:#D9534F;">Logout</a>`;

        navList.appendChild(myBookingsLi);
        navList.appendChild(dashboardLi);
        navList.appendChild(logoutLi);
    } else {
        const loginLi = document.createElement('li');
        loginLi.className = 'nav-auth-item';
        loginLi.innerHTML = `<a href="/pages/login.html">Login</a>`;

        const registerLi = document.createElement('li');
        registerLi.className = 'nav-auth-item';
        registerLi.innerHTML = `<a href="/pages/register.html" class="nav-driver" style="background:var(--color-primary); color:#fff !important; padding:6px 14px; border-radius:4px;">Register</a>`;

        navList.appendChild(loginLi);
        navList.appendChild(registerLi);
    }
}

document.addEventListener('DOMContentLoaded', () => {
    syncNavbarAuth();
});

// Global Toast helper
window.showToast = function(message, type = 'info', duration = 3800) {
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        container.className = 'toast-container';
        document.body.appendChild(container);
    }

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    
    let icon = 'ℹ️';
    if (type === 'success') icon = '✓';
    else if (type === 'error') icon = '✗';
    else if (type === 'warning') icon = '⚠️';

    toast.innerHTML = `<span style="font-weight:bold; margin-right:8px;">${icon}</span><span>${message}</span>`;
    container.appendChild(toast);

    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateX(20px)';
        toast.style.transition = 'all 0.3s ease';
        setTimeout(() => toast.remove(), 300);
    }, duration);
};

window.auth = auth;
window.api = api;
