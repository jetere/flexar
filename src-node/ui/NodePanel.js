// Interactive node add/delete/connect control panel
export class NodeGraphOverlay {
  constructor(canvasId, onGraphChange) {
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas.getContext('2d');
    this.onGraphChange = onGraphChange;

    this.nodes = [];
    this.connections = []; // Array of { fromId, toId }

    this.draggedNode = null;
    this.connectingNode = null;
    this.mouse = { x: 0, y: 0 };

    this.resize();
    this.initDefaultNodes();
    this.attachEvents();
  }

  resize() {
    this.canvas.width = window.innerWidth;
    this.canvas.height = window.innerHeight;
  }

  initDefaultNodes() {
    this.nodes = [
      { id: 'cloud', name: 'Cloud Particles', x: 50, y: 100, width: 160, height: 70, active: true },
      { id: 'wind', name: 'Smooth Wind', x: 260, y: 100, width: 160, height: 70, active: true },
      { id: 'glitch', name: 'Glitch Pass', x: 470, y: 100, width: 160, height: 70, active: false },
      { id: 'output', name: '3D Output', x: 680, y: 100, width: 160, height: 70, active: true }
    ];

    // Initial default connection chain: Cloud -> Wind -> Output
    this.connections = [
      { fromId: 'cloud', toId: 'wind' },
      { fromId: 'wind', toId: 'output' }
    ];
  }

  attachEvents() {
    window.addEventListener('resize', () => this.resize());

    this.canvas.addEventListener('mousedown', (e) => {
      this.mouse.x = e.clientX;
      this.mouse.y = e.clientY;

      for (const node of this.nodes) {
        // Check output socket click (right side socket)
        const outSocketX = node.x + node.width;
        const outSocketY = node.y + node.height / 2;
        if (Math.hypot(this.mouse.x - outSocketX, this.mouse.y - outSocketY) < 12) {
          this.connectingNode = node;
          return;
        }

        // Check node body drag click
        if (
          this.mouse.x >= node.x &&
          this.mouse.x <= node.x + node.width &&
          this.mouse.y >= node.y &&
          this.mouse.y <= node.y + node.height
        ) {
          this.draggedNode = node;
          return;
        }
      }
    });

    this.canvas.addEventListener('mousemove', (e) => {
      this.mouse.x = e.clientX;
      this.mouse.y = e.clientY;

      if (this.draggedNode) {
        this.draggedNode.x = this.mouse.x - this.draggedNode.width / 2;
        this.draggedNode.y = this.mouse.y - this.draggedNode.height / 2;
      }
    });

    this.canvas.addEventListener('mouseup', () => {
      if (this.connectingNode) {
        // Check if mouse was dropped near another node's input socket (left side socket)
        for (const targetNode of this.nodes) {
          if (targetNode.id === this.connectingNode.id) continue;

          const inSocketX = targetNode.x;
          const inSocketY = targetNode.y + targetNode.height / 2;

          if (Math.hypot(this.mouse.x - inSocketX, this.mouse.y - inSocketY) < 20) {
            // Remove existing connections targeting this input
            this.connections = this.connections.filter(c => c.toId !== targetNode.id);
            // Add new connection
            this.connections.push({ fromId: this.connectingNode.id, toId: targetNode.id });

            if (this.onGraphChange) this.onGraphChange(this.getChain());
            break;
          }
        }
      }

      this.draggedNode = null;
      this.connectingNode = null;
    });
  }

  getChain() {
    return this.connections.map(c => ({ from: c.fromId, to: c.toId }));
  }

  draw() {
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    // 1. Draw Connections (Bézier Curves)
    for (const conn of this.connections) {
      const source = this.nodes.find(n => n.id === conn.fromId);
      const target = this.nodes.find(n => n.id === conn.toId);
      if (source && target) {
        this.drawBezierLink(
          source.x + source.width, source.y + source.height / 2,
          target.x, target.y + target.height / 2,
          '#44aaff'
        );
      }
    }

    // 2. Draw Active Connection Line while Dragging
    if (this.connectingNode) {
      this.drawBezierLink(
        this.connectingNode.x + this.connectingNode.width,
        this.connectingNode.y + this.connectingNode.height / 2,
        this.mouse.x, this.mouse.y,
        '#ffaa00'
      );
    }

    // 3. Draw Nodes
    for (const node of this.nodes) {
      const isOutput = node.id === 'output';

      // Node Body
      this.ctx.fillStyle = 'rgba(18, 22, 28, 0.85)';
      this.ctx.strokeStyle = node.active ? '#44aaff' : '#444444';
      this.ctx.lineWidth = 2;
      this.ctx.beginPath();
      this.ctx.roundRect(node.x, node.y, node.width, node.height, 8);
      this.ctx.fill();
      this.ctx.stroke();

      // Label Text
      this.ctx.fillStyle = '#ffffff';
      this.ctx.font = '12px sans-serif';
      this.ctx.fillText(node.name, node.x + 15, node.y + 40);

      // Input Socket (Left - Skip for sources)
      this.ctx.fillStyle = '#44aaff';
      this.ctx.beginPath();
      this.ctx.arc(node.x, node.y + node.height / 2, 5, 0, Math.PI * 2);
      this.ctx.fill();

      // Output Socket (Right - Skip for terminal output)
      if (!isOutput) {
        this.ctx.fillStyle = '#ffaa00';
        this.ctx.beginPath();
        this.ctx.arc(node.x + node.width, node.y + node.height / 2, 5, 0, Math.PI * 2);
        this.ctx.fill();
      }
    }
  }

  drawBezierLink(x1, y1, x2, y2, color) {
    this.ctx.beginPath();
    this.ctx.moveTo(x1, y1);
    const cp1x = x1 + Math.abs(x2 - x1) / 2;
    const cp2x = x2 - Math.abs(x2 - x1) / 2;
    this.ctx.bezierCurveTo(cp1x, y1, cp2x, y2, x2, y2);
    this.ctx.strokeStyle = color;
    this.ctx.lineWidth = 3;
    this.ctx.stroke();
  }
}
