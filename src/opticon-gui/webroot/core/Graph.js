const GRAPH_MARGIN_L = 200;
const GRAPH_MARGIN_R = 20;
const GRAPH_MARGIN_T = 20;
const GRAPH_MARGIN_B = 60;

class Graph {
    constructor (target, width, height) {
        this.target = target;
        this.width = width;
        this.height = height;
        this.initialized = false;
    }
    
    initialize() {
        if (this.initialized) return;
        var ctx = this.target.getContext("2d");
        this.ctx = ctx;
        var fg = ctx.createLinearGradient(0,0,0,200);
        fg.addColorStop (0.00, '#305090c0');
        fg.addColorStop (1.00, '#50c0c0c0');
        var bg = ctx.createLinearGradient(0,0,0,200);
        bg.addColorStop (0.00, '#30509060');
        bg.addColorStop (1.00, '#50c0c060');
        
        this.gradients = { fg: fg, bg: bg };
        ctx.width = this.width * 2;
        ctx.height = this.height * 2;
        ctx.transform (1, 0, 0, -1, 0, this.target.height);
        ctx.scale (0.5, 0.5);
        ctx.clearRect (0, 0, ctx.width, ctx.height);
    }
    
    set(data, max, timespan, unit) {
        // normalize spline to a range of 1000, we will have to scale
        // lookup when drawing.
        
        let numsamples = data.length;
        if (numsamples < 2) return;
        
        let step = 1000 / (numsamples-1);
        let xcoords = [];
        let ycoords = [];
        for (let i=0; i<numsamples; ++i) {
            xcoords.push (i * step);
            ycoords.push (data[i]);
        }
        
        this.graph = {
            max: max,
            timespan: timespan,
            unit: unit,
            data: new Spline (xcoords, ycoords);
        }
    }
    
    drawGraph() {
        if (! this.initialized) return;
        if (! this.graph) return;
        let ctx = this.ctx;
        let data = this.graph.data;
        let gr = this.gradients;
        
        let bwidth = ctx.width - GRAPH_MARGIN_L - GRAPH_MARGIN_R;
        let bheight = ctx.height - GRAPH_MARGIN_T - GRAPH_MARGIN_B;
        
        ctx.clearRect (GRAPH_MARGIN_L, GRAPH_MARGIN_B, bwidth, bheight);
        ctx.beginPath();
        ctx.lineWidth = "1";
        ctx.strokeStyle = "black";
        ctx.rect (GRAPH_MARGIN_L-1, GRAPH_MARGIN_B-1, bwidth+1, bheight+1);
        ctx.stroke();
        
        for (let i=0; i < bwidth; ++i) {
            let x = GRAPH_MARGIN_L + i;
            let y = bheight * (data.at(i) / obj.max);
            ctx.fillStyle = gr.fg;
            ctx.fillRect(x, GRAPH_MARGIN_B, 1, y);
            if (i < (bwidth-1)) {
                ctx.fillStyle = gr.bg;
                ctx.fillRect (x+1, GRAPH_MARGIN_B, 1, y);
            }
        }
    }
}
