const GRAPH_MARGIN_L = 150;
const GRAPH_MARGIN_R = 50;
const GRAPH_MARGIN_T = 30;
const GRAPH_MARGIN_B = 60;

class Graph {
    constructor (target, title, width, height) {
        this.target = target;
        this.title = title;
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
        let unit = this.graph.unit;
        let self = this;

        ctx.clearRect (0, 0, ctx.width, ctx.height);
        
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


        ctx.save();
        ctx.beginPath();

        for (let i=0; i < bwidth; ++i) {
            let index = (1000/bwidth) * i;
            let x = GRAPH_MARGIN_L + i;
            let y = (bheight-1) * (self.graph.data.at(index) / max);
            if (y<0) y=0;
            if (y>bheight) y = bheight;

            if (! i) ctx.moveTo(x, y + GRAPH_MARGIN_B);
            else ctx.lineTo(x, y + GRAPH_MARGIN_B);
        }
                    
        //ctx.strokeStyle = "#ffffff80";
        //ctx.lineWidth = 8;
        //ctx.stroke();
        // 50c0c060
        ctx.strokeStyle = "#000008e0";
        ctx.lineWidth = 2;
        ctx.shadowColor = "#ffffff60";
        ctx.shadowBlur = 20;
        ctx.shadowOffsetX = 0;
        ctx.shadowOffsetY = 0;
        ctx.stroke();
        ctx.restore();

        ctx.beginPath();
        ctx.lineWidth = "2";
        ctx.strokeStyle = "black";
        ctx.rect (GRAPH_MARGIN_L-1, GRAPH_MARGIN_B-1, bwidth+1, bheight+1);
        ctx.stroke();
        
        ctx.font = "24px helvetica";

        var xdat = self.getxaxis();
        for (let datum of xdat) {
            let measure = ctx.measureText (datum.label);
            let w = measure.width;
            let offs = datum.offs + GRAPH_MARGIN_L - (w/2);

            ctx.transform (1, 0, 0, -1, 0, ctx.height);
            ctx.fillStyle = "#282828";
            ctx.fillText (datum.label, offs, ctx.height - 24);
            ctx.transform (1, 0, 0, -1, 0, ctx.height);
            
            ctx.beginPath();
            ctx.moveTo (GRAPH_MARGIN_L + datum.offs, GRAPH_MARGIN_B);
            ctx.lineTo (GRAPH_MARGIN_L + datum.offs, ctx.height - GRAPH_MARGIN_T);
            ctx.strokeStyle = "#ffffff50";
            ctx.stroke();
        }

        let ydat = self.getyaxis();
        for (let datum of ydat) {
            let measure = ctx.measureText (datum.label);
            let w = measure.width;
            
            ctx.transform (1, 0, 0, -1, 0, ctx.height);
            ctx.fillStyle = "#282828";
            ctx.fillText (datum.label, GRAPH_MARGIN_L - (w + 20), datum.offs+8);
            ctx.transform (1, 0, 0, -1, 0, ctx.height);

            let offs = (ctx.height-GRAPH_MARGIN_B) - datum.offs;

            ctx.beginPath();
            ctx.moveTo (GRAPH_MARGIN_L, GRAPH_MARGIN_B+offs);
            ctx.lineTo (ctx.width - GRAPH_MARGIN_R, GRAPH_MARGIN_B+offs);
            ctx.strokeStyle = "#ffffff50";
            ctx.stroke();
        }
        
        if (unit[0] == 'K') {
            if (self.outunit == 'K') self.outunit = 'M';
            else if (self.outunit == 'M') self.outunit = 'G';
            else if (self.outunit == 'G') self.outunit = 'T';
            else self.outunit = 'K';
            unit = self.outunit + unit.substring(1);
        }
        else unit = self.outunit + unit;
        let labelstr = self.title + " " + unit;
        
        let unitmsr = ctx.measureText (labelstr);
        ctx.save();
        ctx.transform (1, 0, 0, -1, 0, ctx.height);
        ctx.translate (30, (ctx.height + unitmsr.width)/2);
        ctx.rotate (-Math.PI/2);
        ctx.fillText (labelstr, 0, 0);
        ctx.restore();
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
            {div:604800,display:"day"},
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
                if ((self.graph.timespan / it.div) < 8) {
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
    
    getyaxis() {
        var matrix = [
            {div:1000000000,display:"G",scale:1000000000},
            {div:100000000,display:"M",scale:1000000},
            {div:10000000,display:"M",scale:1000000},
            {div:1000000,display:"M",scale:1000000},
            {div:100000,display:"K",scale:1000},
            {div:10000,display:"K",scale:1000},
            {div:1000,display:"K",scale:1000},
            {div:100,display:""},
            {div:10,display:""},
            {div:5,display:""},
            {div:1, display:""},
            {div:0.5, display:""},
            {div:0.1, display:""},
            {div:0.05, display:""}
        ];
        let ctx = this.ctx;
        let bwidth = ctx.width - GRAPH_MARGIN_L - GRAPH_MARGIN_R;
        let bheight = ctx.height - GRAPH_MARGIN_T - GRAPH_MARGIN_B;
        let bottomy = GRAPH_MARGIN_T + bheight;
        let self = this;
        let div=1000000000;
        let display = "G";
        let res = [];
        for (let it of matrix) {
            //console.log ("max: "+self.graph.max+" div: "+it.div+" count:"+(self.graph.max/it.div));
            if ((self.graph.max / it.div) > 4) {
                if ((self.graph.max / it.div) > 14) {
                    it.div = it.div * 2;
                }
                //console.log ("match");
                let div = it.div;
                let display = it.display;
                let y = 0;
                
                self.outunit = display;
                
                while (y <= self.graph.max) {
                    let yy = y;
                    if (it.scale) yy = y / it.scale;

                    res.push ({
                        offs: bottomy - (bheight * (y / self.graph.max)),
                        label: yy.toFixed (1)
                    });
                    
                    y = y + (2*div);
                }
                break;
            }
        }
        return res;
    }
}
