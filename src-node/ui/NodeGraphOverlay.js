export class NodeGraphOverlay {
  constructor(containerId, onParamChange, onGraphChange) {
    this.onParamChange = onParamChange;
    this.onGraphChange = onGraphChange;

    // Canvas for Bezier connections
    this.canvas = document.createElement('canvas');
    this.canvas.id = 'node-connections-canvas';
    this.canvas.style.cssText = 'position:absolute;top:0;left:0;width:100vw;height:100vh;z-index:2;pointer-events:none;';
    document.body.appendChild(this.canvas);
    this.ctx = this.canvas.getContext('2d');

    // DOM Container for Node Elements & Controls
    this.nodeContainer = document.createElement('div');
    this.nodeContainer.style.cssText = 'position:absolute;top:0;left:0;width:100vw;height:100vh;z-index:3;pointer-events:none;';
    document.body.appendChild(this.nodeContainer);

    this.nodes = [];
    this.connections = []; // Array of { id, from, to }
    this.draggedNode = null;
    this.connectingFrom = null;
    this.mouse = { x: 0, y: 0 };

    this.resize();
    this.createAddMenuBar();
    this.initDefaultNodes();
    this.attachEvents();
  }

  resize() {
    this.canvas.width = window.innerWidth;
    this.canvas.height = window.innerHeight;
  }

  createAddMenuBar() {
    const bar = document.createElement('div');
    bar.style.cssText = `
      position: absolute; top: 16px; left: 16px; z-index: 10;
      background: rgba(18, 22, 28, 0.95); border: 1px solid #334455;
      border-radius: 8px; padding: 6px 12px; display: flex; gap: 10px;
      align-items: center; font-family: monospace; color: #fff; pointer-events: auto;
    `;

    bar.innerHTML = `
      <span style="font-weight:bold; color:#44aaff;">+ Add Node:</span>
      <button id="add-btn-cloud" style="${this.btnStyle('#44aaff')}">+ Cloud</button>
      <button id="add-btn-wind" style="${this.btnStyle('#00cc88')}">+ Wind</button>
      <button id="add-btn-glitch" style="${this.btnStyle('#ffaa00')}">+ Glitch</button>
    `;

    document.body.appendChild(bar);

    document.getElementById('add-btn-cloud').onclick = () => this.addNode({ name: 'Cloud Particles', type: 'cloud', params: { count: { type: 'number', val: 5000 }, speed: { type: 'number', val: 1.0 } } });
    document.getElementById('add-btn-wind').onclick = () => this.addNode({ name: 'Smooth Wind', type: 'wind', params: { force: { type: 'number', val: 0.8 }, turbulence: { type: 'slider', val: 0.5, min: 0, max: 1, step: 0.05 } } });
    document.getElementById('add-btn-glitch').onclick = () => this.addNode({ name: 'Glitch Pass', type: 'glitch', params: { intensity: { type: 'slider', val: 0.5, min: 0, max: 1, step: 0.01 } } });
  }

  btnStyle(color) {
    return `background: transparent; border: 1px solid ${color}; color: ${color}; padding: 4px 10px; border-radius: 4px; cursor: pointer; font-family: monospace; font-size: 12px;`;
  }

  initDefaultNodes() {
    this.addNode({ id: 'cloud', name: 'Cloud Particles', type: 'cloud', x: 60, y: 120, params: { count: { type: 'number', val: 5000 }, speed: { type: 'number', val: 1.0 } } });
    this.addNode({ id: 'wind', name: 'Smooth Wind', type: 'wind', x: 320, y: 120, params: { force: { type: 'number', val: 0.8 }, turbulence: { type: 'slider', val: 0.5, min: 0, max: 1, step: 0.05 } } });
    this.addNode({ id: 'glitch', name: 'Glitch Pass', type: 'glitch', x: 580, y: 120, params: { intensity: { type: 'slider', val: 0.5, min: 0, max: 1, step: 0.01 } } });
    this.addNode({ id: 'output', name: '3D Output', type: 'output', x: 840, y: 120, params: {} });

    this.connections = [
      { id: 'c1', from: 'cloud', to: 'wind' },
      { id: 'c2', from: 'wind', to: 'output' }
    ];
  }

  addNode(spec) {
    const id = spec.id || `${spec.type}_${Date.now()}`;
    const x = spec.x || Math.random() * 200 + 100;
    const y = spec.y || Math.random() * 200 + 100;

    const nodeData = { id, name: spec.name, type: spec.type, x, y, params: spec.params };

    const nodeEl = document.createElement('div');
    nodeEl.className = 'flexar-node';
    nodeEl.dataset.id = id;
    nodeEl.style.cssText = `
      position: absolute; left: ${x}px; top: ${y}px; width: 190px;
      background: rgba(18, 22, 28, 0.94); border: 1px solid #334455; border-radius: 8px;
      color: #fff; font-family: monospace; padding: 10px; box-shadow: 0 8px 32px rgba(0,0,0,0.5);
      pointer-events: auto; user-select: none;
    `;

    // Header
    const header = document.createElement('div');
    header.style.cssText = 'display:flex;justify-content:space-between;align-items:center;font-weight:bold;margin-bottom:8px;cursor:grab;';
    header.innerHTML = `<span>${spec.name}</span>`;

    if (spec.type !== 'output') {
      const delBtn = document.createElement('span');
      delBtn.innerText = '✕';
      delBtn.style.cssText = 'cursor:pointer;color:#ff4444;font-size:12px;';
      delBtn.onclick = (e) => {
        e.stopPropagation();
        this.removeNode(id);
      };
      header.appendChild(delBtn);
    }
    nodeEl.appendChild(header);

    // Dynamic Parameter Controls (Inputs vs Sliders)
    const controlsContainer = document.createElement('div');
    for (const [key, prop] of Object.entries(spec.params)) {
      const row = document.createElement('div');
      row.style.cssText = 'display:flex;justify-content:space-between;align-items:center;margin-top:6px;font-size:11px;';

      const label = document.createElement('span');
      label.style.color = '#88aaff';
      label.innerText = key.toUpperCase();
      row.appendChild(label);

      if (prop.type === 'number') {
        // Value Box Input
        const input = document.createElement('input');
        input.type = 'number';
        input.value = prop.val;
        input.style.cssText = 'width:60px;background:#11161d;border:1px solid #334455;color:#fff;border-radius:4px;padding:2px 4px;font-family:monospace;text-align:right;';
        input.onchange = (e) => {
          const val = parseFloat(e.target.value) || 0;
          prop.val = val;
          if (this.onParamChange) this.onParamChange(id, key, val);
        };
        row.appendChild(input);
      } else if (prop.type === 'slider') {
        // Reserved Slider Input
        const sliderContainer = document.createElement('div');
        sliderContainer.style.cssText = 'display:flex;align-items:center;gap:6px;';

        const slider = document.createElement('input');
        slider.type = 'range';
        slider.min = prop.min;
        slider.max = prop.max;
        slider.step = prop.step;
        slider.value = prop.val;
        slider.style.cssText = 'width:70px;accent-color:#44aaff;cursor:pointer;';

        const valSpan = document.createElement('span');
        valSpan.innerText = prop.val;
        valSpan.style.cssText = 'width:24px;text-align:right;font-size:10px;';

        slider.oninput = (e) => {
          const val = parseFloat(e.target.value);
          prop.val = val;
          valSpan.innerText = val;
          if (this.onParamChange) this.onParamChange(id, key, val);
        };

        sliderContainer.appendChild(slider);
        sliderContainer.appendChild(valSpan);
        row.appendChild(sliderContainer);
      }

      controlsContainer.appendChild(row);
    }
    nodeEl.appendChild(controlsContainer);

    // Input Socket
    if (spec.type !== 'cloud') {
      const inSocket = document.createElement('div');
      inSocket.style.cssText = 'position:absolute;left:-8px;top:50%;width:12px;height:12px;border-radius:50%;background:#44aaff;cursor:pointer;transform:translateY(-50%);';
      inSocket.onclick = () => this.connectOrEject(id, 'in');
      nodeEl.appendChild(inSocket);
    }

    // Output Socket
    if (spec.type !== 'output') {
      const outSocket = document.createElement('div');
      outSocket.style.cssText = 'position:absolute;right:-8px;top:50%;width:12px;height:12px;border-radius:50%;background:#ffaa00;cursor:pointer;transform:translateY(-50%);';
      outSocket.onclick = () => this.connectOrEject(id, 'out');
      nodeEl.appendChild(outSocket);
    }

    // Node Drag
    header.onmousedown = (e) => {
      this.draggedNode = { spec: nodeData, el: nodeEl, offsetX: e.clientX - nodeData.x, offsetY: e.clientY - nodeData.y };
    };

    this.nodeContainer.appendChild(nodeEl);
    this.nodes.push(nodeData);
    this.drawConnections();
  }

  // Socket Eject & Reconnect logic
  connectOrEject(nodeId, socketType) {
    if (socketType === 'out') {
      // Start cable connection from output socket
      this.connectingFrom = nodeId;
    } else if (socketType === 'in') {
      // If wire actively being dragged, attach it
      if (this.connectingFrom && this.connectingFrom !== nodeId) {
        this.connections = this.connections.filter(c => c.to !== nodeId); // Eject previous connection to this input
        this.connections.push({ id: `c_${Date.now()}`, from: this.connectingFrom, to: nodeId });
        this.connectingFrom = null;
        this.drawConnections();
        if (this.onGraphChange) this.onGraphChange(this.connections);
      } else {
        // Eject existing wire if input clicked without dynamic wire active
        const existingIndex = this.connections.findIndex(c => c.to === nodeId);
        if (existingIndex !== -1) {
          const ejected = this.connections.splice(existingIndex, 1)[0];
          this.connectingFrom = ejected.from; // Take over the unplugged wire to reconnect elsewhere
          this.drawConnections();
          if (this.onGraphChange) this.onGraphChange(this.connections);
        }
      }
    }
  }

  removeNode(id) {
    this.nodes = this.nodes.filter(n => n.id !== id);
    this.connections = this.connections.filter(c => c.from !== id && c.to !== id);
    const el = this.nodeContainer.querySelector(`[data-id="${id}"]`);
    if (el) el.remove();
    this.drawConnections();
    if (this.onGraphChange) this.onGraphChange(this.connections);
  }

  attachEvents() {
    window.addEventListener('resize', () => this.resize());

    window.addEventListener('mousemove', (e) => {
      this.mouse.x = e.clientX;
      this.mouse.y = e.clientY;

      if (this.draggedNode) {
        this.draggedNode.spec.x = e.clientX - this.draggedNode.offsetX;
        this.draggedNode.spec.y = e.clientY - this.draggedNode.offsetY;
        this.draggedNode.el.style.left = `${this.draggedNode.spec.x}px`;
        this.draggedNode.el.style.top = `${this.draggedNode.spec.y}px`;
        this.drawConnections();
      } else if (this.connectingFrom) {
        this.drawConnections();
      }
    });

    // Right click directly on background to drop dynamic wire
    window.addEventListener('click', (e) => {
      if (e.target.tagName === 'CANVAS' && this.connectingFrom) {
        this.connectingFrom = null; // Eject/cancel wire
        this.drawConnections();
      }
    });

    window.addEventListener('mouseup', () => {
      this.draggedNode = null;
    });
  }

  drawConnections() {
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    for (const conn of this.connections) {
      const fromNode = this.nodes.find(n => n.id === conn.from);
      const toNode = this.nodes.find(n => n.id === conn.to);
      if (fromNode && toNode) {
        this.drawBezier(fromNode.x + 190, fromNode.y + 35, toNode.x, toNode.y + 35, '#44aaff');
      }
    }

    if (this.connectingFrom) {
      const fromNode = this.nodes.find(n => n.id === this.connectingFrom);
      if (fromNode) {
        this.drawBezier(fromNode.x + 190, fromNode.y + 35, this.mouse.x, this.mouse.y, '#ffaa00');
      }
    }
  }

  drawBezier(x1, y1, x2, y2, color) {
    this.ctx.beginPath();
    this.ctx.moveTo(x1, y1);
    const dx = Math.abs(x2 - x1) / 2;
    this.ctx.bezierCurveTo(x1 + dx, y1, x2 - dx, y2, x2, y2);
    this.ctx.strokeStyle = color;
    this.ctx.lineWidth = 3;
    this.ctx.stroke();
  }

  draw() {
    this.drawConnections();
  }
}
