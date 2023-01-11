const GRAPH_MARGIN_L = 150;
const GRAPH_MARGIN_R = 50;
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
        var fg = ctx.createLinearGradient(0,0,0,this.height);
        fg.addColorStop (0.00, '#305090c0');
        fg.addColorStop (1.00, '#50c0c0c0');
        var bg = ctx.createLinearGradient(0,0,0,this.height);
        bg.addColorStop (0.00, '#30509060');
        bg.addColorStop (1.00, '#50c0c060');
        var fil = ctx.createLinearGradient(0,0,0,this.height);
        fil.addColorStop (0.00, '#585858ff');
        fil.addColorStop (1.00, '#383838ff');
        
        this.gradients = { fg: fg, bg: bg, fil: fil };
        ctx.width = this.width * 2;
        ctx.height = this.height * 2;
        ctx.transform (1, 0, 0, -1, 0, this.target.height);
        ctx.scale (0.5, 0.5);
        ctx.clearRect (0, 0, ctx.width, ctx.height);
        
        this.initialized = true;
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
            data: new Spline (xcoords, ycoords),
            in: data
        }
    }
    
    drawGraph() {
        if (! this.initialized) return;
        if (! this.graph) return;
        let ctx = this.ctx;
        let max = this.graph.max;
        let gr = this.gradients;
        let self = this;
        
        let bwidth = ctx.width - GRAPH_MARGIN_L - GRAPH_MARGIN_R;
        let bheight = ctx.height - GRAPH_MARGIN_T - GRAPH_MARGIN_B;
        
        ctx.beginPath();
        ctx.lineWidth = "2";
        ctx.strokeStyle = "black";
        ctx.rect (GRAPH_MARGIN_L-1, GRAPH_MARGIN_B-1, bwidth+1, bheight+1);
        ctx.stroke();

        ctx.fillStyle = gr.fil;
        ctx.fillRect (GRAPH_MARGIN_L, GRAPH_MARGIN_B, bwidth, bheight);
        
        for (let i=0; i < bwidth; ++i) {
            let index = (1000/bwidth) * i;
            let x = GRAPH_MARGIN_L + i;
            let y = (bheight-1) * (self.graph.data.at(index) / max);
            if (y<0) y=0;
            if (y>bheight) y = bheight;

            ctx.fillStyle = gr.fg;
            ctx.fillRect(x, GRAPH_MARGIN_B, 1, y);
            if (i < (bwidth-1)) {
                ctx.fillStyle = gr.bg;
                ctx.fillRect (x+1, GRAPH_MARGIN_B, 1, y);
            }
        }

        ctx.beginPath();
        ctx.lineWidth = "2";
        ctx.strokeStyle = "black";
        ctx.rect (GRAPH_MARGIN_L-1, GRAPH_MARGIN_B-1, bwidth+1, bheight+1);
        ctx.stroke();
    }
    
    mklabel(dt, display) {
        if (display == "hm") {
            return dt.getHours() + ":" +
                   dt.getMinutes().toString().padStart(2, '0');
        }
        else {
            return dt.getDate() + "-" +
                   (1+dt.getMonth()).toString().padStart(2, '0');
        }
    }
    
    getxaxis() {
        var matrix = [
            {div:86400,display:"day"},
            {div:21600,display:"hm"},
            {div:10800,display:"hm"},
            {div:3600,display:"hm"},
            {div:300,display:"hm"}
        ];
        let ctx = this.ctx;
        let bwidth = ctx.width - GRAPH_MARGIN_L - GRAPH_MARGIN_R;
        let self = this;
        let div=86400;
        let display = "day";
        let res = [];
        for (let it of matrix) {
            if ((self.graph.timespan / it.div) > 1) {
                if ((self.graph.timespan / it.div) < 6) {
                    let div = it.div;
                    let display = it.display;
                    
                    let dt = new Date();
                    let tnow = dt.getTime()/1000;
                    let x = tnow - (tnow % div);
                    while (x > (tnow - self.graph.timespan)) {
                        let outd = new Date();
                        outd.setTime (x*1000);
                        let offs = bwidth * ((tnow-x)/self.graph.timespan);
                        offs = bwidth - offs;
                        res.push({
                            offs: offs,
                            label: self.mklabel (outd, display)
                        });
                        
                        x = x - div;
                    }
                    break;
                }
            }
        }
        return res;
    }
}
