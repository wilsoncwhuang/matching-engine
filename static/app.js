// Application state
let isAutoPlaying = false;
let autoPlayInterval = null;
let currentSymbol = 'AAPL';

// DOM elements
const stepBtn = document.getElementById('stepBtn');
const autoBtn = document.getElementById('autoBtn');
const resetBtn = document.getElementById('resetBtn');
const symbolSelect = document.getElementById('symbolSelect');
const symbolTitle = document.getElementById('symbolTitle');
const stepInfo = document.getElementById('stepInfo');
const tradeInfo = document.getElementById('tradeInfo');
const currentCommand = document.getElementById('currentCommand');
const spreadValue = document.getElementById('spreadValue');
const tradesContainer = document.getElementById('tradesContainer');
const totalVolume = document.getElementById('totalVolume');
const avgPrice = document.getElementById('avgPrice');
const minPrice = document.getElementById('minPrice');
const maxPrice = document.getElementById('maxPrice');
const priceStd = document.getElementById('priceStd');

// Event listeners
stepBtn.addEventListener('click', executeStep);
autoBtn.addEventListener('click', toggleAutoPlay);
resetBtn.addEventListener('click', resetDemo);
symbolSelect.addEventListener('change', (e) => {
    currentSymbol = e.target.value;
    symbolTitle.textContent = currentSymbol;
    loadState();
});

// Initialize
loadState();

async function executeStep() {
    try {
        const response = await fetch(`/api/step?symbol=${currentSymbol}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        });
        
        const data = await response.json();
        
        if (data.status === 'completed') {
            currentCommand.textContent = 'All steps are completed!';
            currentCommand.className = 'command-display completed';
            stopAutoPlay();
            return;
        }
        
        if (data.status === 'error') {
            currentCommand.textContent = `Error: ${data.message}`;
            currentCommand.className = 'command-display error';
            return;
        }
        
        // Update command display
        updateCommandDisplay(data);
        
        // Update current symbol based on the operation
        let symbolChanged = false;
        if (data.current_symbol && data.current_symbol !== currentSymbol) {
            symbolChanged = true;
            currentSymbol = data.current_symbol;
            symbolTitle.textContent = currentSymbol;
            symbolSelect.value = currentSymbol;
        }
        
        // Update orderbook
        if (data.orderbook) {
            updateOrderBook(data.orderbook);
        }
        
        // If symbol changed, call loadState to get correct volume
        if (symbolChanged) {
            await loadState();
            return;  
        }
        
        // Update trades
        if (data.recent_trades) {
            updateTrades(data.recent_trades);
        }
        
        // Update statistics
        updateStats(data);
        
        // Update total volume directly (along with orderbook)
        if (data.total_volume !== undefined) {
            totalVolume.textContent = data.total_volume || 0;
        }
        
        // Update progress
        stepInfo.textContent = `Steps: ${data.step + 1} / ${data.total_steps}`;
        tradeInfo.textContent = `Trades: ${data.total_trades || 0}`;
        
    } 
    catch (error) {
        console.error('Error executing step:', error);
        currentCommand.textContent = `Error: ${error.message}`;
        currentCommand.className = 'command-display error';
    }
}

function updateCommandDisplay(data) {
    let icon = '';
    let text = '';
    let className = 'command-display';
    
    if (data.action === 'new_order') {
        text = `${icon} new order #${data.order_id}: ${data.side} ${data.quantity} @ ${data.price}`;
        className += data.side === 'BUY' ? ' buy' : ' sell';
    } 
    else if (data.action === 'cancel') {
        text = `${icon} cancel order #${data.order_id}`;
        className += ' cancel';
    } 
    else if (data.action === 'modify') {
        text = `${icon} modify order #${data.order_id}`;
        className += ' modify';
    } 
    else if (data.action === 'volume_report') {
        text = `${icon} volume report - total: ${data.total_volume}`;
        className += ' report';
    }
    
    currentCommand.textContent = text;
    currentCommand.className = className;
    
    currentCommand.style.animation = 'none';
    setTimeout(() => {
        currentCommand.style.animation = 'slideIn 0.3s ease-out';
    }, 10);
}

function updateOrderBook(orderbook) {
    // calculate max quantity for bar width calculation
    let maxQty = 0;
    let totalBidQty = 0;
    let totalAskQty = 0;
    if (orderbook.asks) {
        orderbook.asks.forEach(level => {
            maxQty = Math.max(maxQty, level.quantity);
            totalAskQty += level.quantity;
        });
    }
    if (orderbook.bids) {
        orderbook.bids.forEach(level => {
            maxQty = Math.max(maxQty, level.quantity);
            totalBidQty += level.quantity;
        });
    }
    
    // update five levels of asks 
    for (let i = 1; i <= 5; i++) {
        const askPriceEl = document.getElementById(`ask${i}-price`);
        const askQtyEl = document.getElementById(`ask${i}-qty`);
        const askBarEl = document.getElementById(`ask${i}-bar`);
        
        if (orderbook.asks && orderbook.asks.length >= i) {
            const level = orderbook.asks[i - 1];
            askPriceEl.textContent = level.price.toFixed(2);
            askQtyEl.textContent = level.quantity;
            askPriceEl.classList.add('has-data');
            
            // update bar width
            const widthPercent = maxQty > 0 ? (level.quantity / maxQty * 100) : 0;
            askBarEl.style.width = widthPercent + '%';
        } 
        else {
            askPriceEl.textContent = '---';
            askQtyEl.textContent = '---';
            askPriceEl.classList.remove('has-data');
            askBarEl.style.width = '0%';
        }
    }
    
    // update five levels of bids 
    for (let i = 1; i <= 5; i++) {
        const bidPriceEl = document.getElementById(`bid${i}-price`);
        const bidQtyEl = document.getElementById(`bid${i}-qty`);
        const bidBarEl = document.getElementById(`bid${i}-bar`);
        
        if (orderbook.bids && orderbook.bids.length >= i) {
            const level = orderbook.bids[i - 1];
            bidPriceEl.textContent = level.price.toFixed(2);
            bidQtyEl.textContent = level.quantity;
            bidPriceEl.classList.add('has-data');
            
            // update bar width
            const widthPercent = maxQty > 0 ? (level.quantity / maxQty * 100) : 0;
            bidBarEl.style.width = widthPercent + '%';
        } 
        else {
            bidPriceEl.textContent = '---';
            bidQtyEl.textContent = '---';
            bidPriceEl.classList.remove('has-data');
            bidBarEl.style.width = '0%';
        }
    }
    
    // update totals
    document.getElementById('totalBidQty').textContent = totalBidQty;
    document.getElementById('totalAskQty').textContent = totalAskQty;
}

function updateTrades(trades) {
    if (trades.length === 0) {
        tradesContainer.innerHTML = '<div class="empty-state">No trades yet</div>';
        return;
    }
    
    tradesContainer.innerHTML = trades.map(trade => {
        let timeStr = '';
        if (trade.timestamp !== undefined) {
            // Format system clock timestamp (seconds since epoch) to readable time
            const date = new Date(trade.timestamp * 1000);
            const hours = String(date.getHours()).padStart(2, '0');
            const minutes = String(date.getMinutes()).padStart(2, '0');
            const seconds = String(date.getSeconds()).padStart(2, '0');
            timeStr = `${hours}:${minutes}:${seconds}`;
        }
        return `
        <div class="trade-item">
            <div class="trade-header">
                <span class="trade-id">#${trade.trade_id}</span>
                <span class="trade-price">@ ${trade.price.toFixed(2)}</span>
                ${timeStr ? `<span class="trade-time">${timeStr}</span>` : ''}
            </div>
            <div class="trade-details">
                <span>Quantity: ${trade.quantity}</span>
                <span class="trade-orders">
                    Buy Order #${trade.buy_order_id} × Sell Order #${trade.sell_order_id}
                </span>
            </div>
        </div>
    `}).join('');
}

function updateStats(data) {
    if (data.total_volume !== undefined) {
        totalVolume.textContent = data.total_volume || 0;
        
        // Add animation
        totalVolume.style.animation = 'none';
        setTimeout(() => {
            totalVolume.style.animation = 'pulse 0.5s ease-out';
        }, 10);
    }
    
    // Update price statistics
    if (data.avg_price !== undefined) {
        avgPrice.textContent = data.avg_price !== null ? data.avg_price.toFixed(2) : '-';
    }
    if (data.min_price !== undefined) {
        minPrice.textContent = data.min_price !== null ? data.min_price.toFixed(2) : '-';
    }
    if (data.max_price !== undefined) {
        maxPrice.textContent = data.max_price !== null ? data.max_price.toFixed(2) : '-';
    }
    if (data.price_std !== undefined) {
        priceStd.textContent = data.price_std !== null ? data.price_std.toFixed(4) : '-';
    }
}

function toggleAutoPlay() {
    if (isAutoPlaying) {
        stopAutoPlay();
    } else {
        startAutoPlay();
    }
}

function startAutoPlay() {
    isAutoPlaying = true;
    autoBtn.textContent = 'Stop';
    autoBtn.classList.add('active');
    
    autoPlayInterval = setInterval(async () => {
        await executeStep();
    }, 1500); 
}

function stopAutoPlay() {
    isAutoPlaying = false;
    autoBtn.textContent = 'Auto';
    autoBtn.classList.remove('active');
    
    if (autoPlayInterval) {
        clearInterval(autoPlayInterval);
        autoPlayInterval = null;
    }
}

async function resetDemo() {
    stopAutoPlay();
    
    try {
        const response = await fetch('/api/reset', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        });
        
        const data = await response.json();
        
        if (data.status === 'success') {
            // Reset UI
            currentCommand.textContent = 'Reset complete. Ready to start again!';
            currentCommand.className = 'command-display';
            
            // Reset five levels
            for (let i = 1; i <= 5; i++) {
                document.getElementById(`ask${i}-price`).textContent = '---';
                document.getElementById(`ask${i}-qty`).textContent = '---';
                document.getElementById(`ask${i}-bar`).style.width = '0%';
                document.getElementById(`bid${i}-price`).textContent = '---';
                document.getElementById(`bid${i}-qty`).textContent = '---';
                document.getElementById(`bid${i}-bar`).style.width = '0%';
            }
            
            document.getElementById('totalBidQty').textContent = '0';
            document.getElementById('totalAskQty').textContent = '0';
            
            tradesContainer.innerHTML = '<div class="empty-state">No trades yet</div>';
            totalVolume.textContent = '0';
            avgPrice.textContent = '-';
            minPrice.textContent = '-';
            maxPrice.textContent = '-';
            priceStd.textContent = '-';
            stepInfo.textContent = 'Steps: 0 / 0';
            tradeInfo.textContent = 'Trades: 0';
        }
    } 
    catch (error) {
        console.error('Error resetting demo:', error);
        alert('Reset failed: ' + error.message);
    }
}

async function loadState() {
    try {
        const response = await fetch(`/api/state?symbol=${currentSymbol}`);
        const data = await response.json();
        
        stepInfo.textContent = `Steps: ${data.current_step} / ${data.total_steps}`;
        tradeInfo.textContent = `Trades: ${data.total_trades}`;
        
        updateStats(data);
        
        if (data.orderbook) {
            updateOrderBook(data.orderbook);
        }
        
        if (data.recent_trades) {
            updateTrades(data.recent_trades);
        }
    } 
    catch (error) {
        console.error('Error loading state:', error);
    }
}
