// Cashew Network Web UI - JavaScript

// Configuration
const API_BASE = '';  // Same origin
const WS_URL = 'ws://localhost:8080/ws';  // WebSocket endpoint

// State
let ws = null;
let authenticated = false;
let userKey = null;
let networks = [];

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    initTabs();
    connectWebSocket();
    loadNetworks();
    loadGatewayStats();
    
    // Periodic updates
    setInterval(loadGatewayStats, 10000);  // Every 10 seconds
});

// Tab switching
function initTabs() {
    const tabs = document.querySelectorAll('.tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const tabName = tab.dataset.tab;
            switchTab(tabName);
        });
    });
}

function switchTab(tabName) {
    // Update tab buttons
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
    
    // Update content
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    document.getElementById(`${tabName}-tab`).classList.add('active');
}

// WebSocket connection
function connectWebSocket() {
    // In production, implement actual WebSocket connection
    updateStatus('online', 'Connected');
    console.log('WebSocket: Would connect to', WS_URL);
}

function updateStatus(state, text) {
    const statusEl = document.getElementById('status');
    statusEl.className = `status-indicator ${state}`;
    statusEl.querySelector('.text').textContent = text;
}

// Load networks
async function loadNetworks() {
    try {
        const response = await fetch(`${API_BASE}/api/networks`);
        const data = await response.json();
        
        networks = data.networks || [];
        renderNetworks(networks);
    } catch (error) {
        console.error('Failed to load networks:', error);
        document.getElementById('networks-list').innerHTML = 
            '<p class="placeholder">Failed to load networks. Is the gateway running?</p>';
    }
}

function renderNetworks(networks) {
    const container = document.getElementById('networks-list');
    
    if (networks.length === 0) {
        container.innerHTML = '<p class="placeholder">No networks available yet.</p>';
        return;
    }
    
    container.innerHTML = networks.map(net => `
        <div class="network-card" onclick="viewNetwork('${net.id}')">
            <h3>${net.name || 'Network ' + net.id.substring(0, 8)}</h3>
            <p class="meta">📍 ID: ${net.id.substring(0, 16)}...</p>
            <p class="meta">👥 Members: ${net.member_count || 0}</p>
            <p class="meta">📦 Thing: ${net.thing_hash ? net.thing_hash.substring(0, 16) + '...' : 'N/A'}</p>
        </div>
    `).join('');
}

function viewNetwork(networkId) {
    alert(`Viewing network: ${networkId}\n\nFull network details coming soon!`);
}

// Search content
async function searchContent() {
    const query = document.getElementById('content-search').value.trim();
    if (!query) {
        return;
    }
    
    const container = document.getElementById('content-list');
    container.innerHTML = '<div class="loading">Searching P2P network...</div>';
    
    try {
        const response = await fetch(`${API_BASE}/api/things/${query}`);
        
        if (response.ok) {
            const blob = await response.blob();
            const contentType = response.headers.get('Content-Type');
            
            container.innerHTML = `
                <div class="content-item">
                    <div class="icon">📄</div>
                    <h3>Content Found</h3>
                    <p>Type: ${contentType}</p>
                    <p>Size: ${blob.size} bytes</p>
                    <button onclick="downloadContent('${query}')">Download</button>
                </div>
            `;
        } else {
            container.innerHTML = '<p class="placeholder">Content not found in network.</p>';
        }
    } catch (error) {
        console.error('Search error:', error);
        container.innerHTML = '<p class="placeholder">Search failed. Check network connection.</p>';
    }
}

function downloadContent(hash) {
    window.open(`${API_BASE}/api/things/${hash}`, '_blank');
}

// Authentication
function showAuthOptions() {
    alert('Authentication options:\n\n1. Generate PoW key (Start Mining)\n2. Import existing key (Coming soon)\n3. Use browser wallet (Coming soon)');
}

function logout() {
    authenticated = false;
    userKey = null;
    document.getElementById('anonymous-state').style.display = 'block';
    document.getElementById('authenticated-state').style.display = 'none';
    updateCapabilities();
}

// Proof of Work
let powWorker = null;

function startPoW() {
    const button = document.getElementById('pow-button');
    const progress = document.getElementById('pow-progress');
    
    button.disabled = true;
    button.textContent = 'Mining...';
    progress.style.display = 'block';
    
    // Simulate PoW (in production, use Web Worker)
    let percent = 0;
    const interval = setInterval(() => {
        percent += Math.random() * 5;
        if (percent >= 100) {
            percent = 100;
            clearInterval(interval);
            completePoW();
        }
        
        document.getElementById('pow-fill').style.width = percent + '%';
        document.getElementById('pow-status').textContent = 
            `Mining... ${Math.floor(percent)}%`;
    }, 500);
}

function completePoW() {
    // Generate mock key
    userKey = Array.from({length: 32}, () => 
        Math.floor(Math.random() * 256).toString(16).padStart(2, '0')
    ).join('');
    
    authenticated = true;
    
    document.getElementById('anonymous-state').style.display = 'none';
    document.getElementById('authenticated-state').style.display = 'block';
    document.getElementById('user-key').textContent = userKey.substring(0, 16) + '...';
    
    document.getElementById('pow-button').disabled = false;
    document.getElementById('pow-button').textContent = 'Start Mining';
    document.getElementById('pow-progress').style.display = 'none';
    
    updateCapabilities();
    
    alert('✅ Key generated successfully!\n\nYou can now post content and vote.');
}

function updateCapabilities() {
    document.getElementById('cap-post').innerHTML = authenticated ? 
        '✓ Post content (active)' : '✗ Post content (requires key)';
    document.getElementById('cap-vote').innerHTML = authenticated ?
        '✓ Vote on content (active)' : '✗ Vote on content (requires key)';
    document.getElementById('cap-host').innerHTML = authenticated ?
        '⏳ Host networks (requires reputation)' : '✗ Host networks (requires key + reputation)';
}

// Gateway stats
async function loadGatewayStats() {
    try {
        const response = await fetch(`${API_BASE}/api/status`);
        const data = await response.json();
        
        const statsHtml = `
            <p><strong>Total Requests:</strong> ${data.total_requests || 0}</p>
            <p><strong>Active Sessions:</strong> ${data.active_sessions || 0}</p>
            <p><strong>Anonymous:</strong> ${data.anonymous_sessions || 0}</p>
            <p><strong>Authenticated:</strong> ${data.authenticated_sessions || 0}</p>
            <p><strong>Bytes Sent:</strong> ${formatBytes(data.bytes_sent || 0)}</p>
            <p><strong>Bytes Received:</strong> ${formatBytes(data.bytes_received || 0)}</p>
        `;
        
        document.getElementById('gateway-stats').innerHTML = statsHtml;
    } catch (error) {
        console.error('Failed to load stats:', error);
        document.getElementById('gateway-stats').innerHTML = 
            '<p class="placeholder">Stats unavailable</p>';
    }
}

function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}

// Event listeners for Enter key
document.getElementById('content-search')?.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        searchContent();
    }
});
