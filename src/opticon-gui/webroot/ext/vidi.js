// ----------------------------------------------------------------------------
// vidi.js: A compact implementation of a vuejs-style templating engine for
//          javascript without the whole cognitive load of a framework.
//
// Copyright (c) 2020 Pim van Riezen <pi@madscience.nl>
// Provided under the GNU Library General Public License version 2 or newer
// ----------------------------------------------------------------------------

let SafeArrayMethods = {
    indexOf:true,
    lastIndexOf:true,
    find:true,
    findIndex:true,
    includes:true,
    entries:true,
    every:true,
    forEach:true,
    values:true,
    valueOf:true,
    map:true,
    forEach:true,
    join:true
};

// ============================================================================
// CLASS NestProxy
// ---------------
// A variation of the JavaScript proxy object that survives nested
// hierarchies. The provided handler should implement a setPath() and
// getPath() function that handle actual access to the proxied data.
// ============================================================================
class NestProxy {
    constructor (target,handler,parent,key) {
        this.target = target;
        this.handler = handler;
        this.parent = parent ? parent : null;
        this.key = key ? key : null;
        this.subtrees = {};
        
        // Javascript class definitions use get/set for getters/setters,
        // but the javascript Proxy API demands an object to have members
        // with those exact names. So we cheese it.
        this.get = this.getProperty;
        this.set = this.setProperty;
    }
    
    // ------------------------------------------------------------------------
    // Returns an array of the path inside the object tree for this
    // particular instance
    // ------------------------------------------------------------------------
    keypath(plus) {
        let res = [];
        if (this.parent) {
            res = this.parent.keypath();
            res.push (this.key);
        }
        if (plus) res.push (plus);
        return res;
    }
    
    // ------------------------------------------------------------------------
    // Callback from the proxy object for getting a property
    // ------------------------------------------------------------------------
    getProperty(obj, prop) {
        //if (Vidi.debug) console.log ("getProperty",obj,prop);
    
        if (this.subtrees[prop]) return this.subtrees[prop].p;
        if (Array.isArray (this.target)) {
            if (typeof(prop) != 'number' && typeof(prop) != 'string') {
                return this.target[prop];
            }
            if (typeof (prop) == "string") {
                if (typeof(Array.prototype[prop]) == "function") {
                    let self = this;
                    return function() {
                        let res = Array.prototype[prop].apply (self.target,arguments);
                        // Skip methods that have no side-effects.
                        if (! SafeArrayMethods[prop]) {
                            self.parent.setProperty (obj, self.key, self.target);
                        }
                        return res;
                    }
                }
                else {
                    return this.target[prop];
                }
            }
        }
        let robj = this;
        while (robj.parent) robj = robj.parent;
        let handled = this.handler.getPath (robj.target, this.keypath(prop));
        if (handled === null || handled === undefined) return handled;
        if (typeof (handled) !== "object") return handled;
        if (handled instanceof Element) return handled;
        if (! this.target[prop]) {
            if (Array.isArray (handled)) this.target[prop] = [];
            else this.target[prop] = {};        
        }
        let dp = new NestProxy (this.target[prop], this.handler, this, prop);
        let p = new Proxy (this.target[prop], dp);
        this.subtrees[prop] = {p:p,dp:dp};
        return p;
    }
    
    // ------------------------------------------------------------------------
    // Callback from the proxy object for setting a property
    // ------------------------------------------------------------------------
    setProperty(obj, prop, value, ignore, ischild) {
        if (Vidi.debug) console.trace ("setProperty",obj,prop,value);
        let isobj = (typeof (value) == "object");
        let isarr = Array.isArray (value);
        let targetarr = Array.isArray (this.target);
        
        if (isarr && this.target[prop] && Array.isArray (this.target[prop])) {
            if (value.length == 0 && this.subtrees[prop]) {
                this.subtrees[prop].p.length = 0;
                return true;
            }
        }
        
        if (isobj && (value === null || value === undefined)) {
            isobj = false;
        }
        
        if (this.subtrees[prop]) {
            if (isarr) {
                this.subtrees[prop].p.length = value.length;
            }
            else {
                delete this.subtrees[prop];
            }
        }
        
        let robj = this;
        while (robj.parent) robj = robj.parent;
        
        if (targetarr && prop != '0' && !parseInt(prop)) {
            if (prop == "length") {
                this.target.length = value;
                if (this.parent) {
                    this.parent.target[this.key] = this.target;
                    this.parent.handler.setPath (this.parent.target,
                                                 this.keypath(),
                                                 this.target);
                }
            }
            return true;
        }

        if (! isobj) {
            if (ischild) {
                this.target[prop] = value;
                return true;
            }
            return this.handler.setPath (robj.target, this.keypath(prop), value);
        }

        if (isarr) {
            if (Array.isArray (this.target[prop])) {
                this.target[prop].length = value.length;
            }
            else this.target[prop] = [];
        }
        else this.target[prop] = {};
        
        let subtree = this.subtrees[prop];
        if (! subtree) {
            let dp = new NestProxy (this.target[prop], this.handler, this, prop);
            let p = new Proxy (this.target[prop], dp);
            subtree = this.subtrees[prop] = {p:p,dp:dp};
        }
        
        for (let k in value) {
            subtree.dp.setProperty (obj, k, value[k], null, true);
        }
        
        if (! ischild) {
            this.target[prop] = value;
            return this.handler.setPath (robj.target, this.keypath(prop), value);
        }
        return true;
    }
    
    // ------------------------------------------------------------------------
    // Callback from the proxy object for deleting a property
    // ------------------------------------------------------------------------
    deleteProperty(obj,prop) {
        if (this.subtrees[prop]) {
            delete this.subtrees[prop];
        }
        
        let robj = this;
        while (robj.parent) robj = robj.parent;
        return this.handler.deletePath (robj.target, this.keypath(prop));
    }
}

// ============================================================================
// Because javascript lacks actual convenient introspection, the native
// Proxy object is actually magical, so it needs to wrap itself against
// -our- proxy, which wrapped itself against the data object. This function
// performs this double wrapping for convenience sake.
// ============================================================================
NestProxy.create = function(target,handler) {
    return new Proxy (target, new NestProxy (target, handler));
}

// ============================================================================
// CLASS VidiView
// --------------
// Wraps a sub-tree of the browser DOM into a Vidi template, dynamically
// rendered according to the data put into its view member-object.
// ============================================================================
class VidiView
{
    // ------------------------------------------------------------------------
    // Constructor. Expects the id of the DOM node that is to be our template,
    // and a model-definition of what should go into the view.
    // ------------------------------------------------------------------------
    constructor(id,def) {
        let self = this;
        let model = def.model;
        
        for (let varname in model) {
            if (! varname.match (/^[a-zA-Z0-9_]+$/)) {
                console.error("[Vidi] model for view '#"+id+"' defines "+
                              "invalid top-level variable '"+varname+"'");
                throw new Error("Invalid model");
            }
        }
        
        for (let varname in def.methods) {
            if (! varname.match (/^[a-zA-Z0-9_]+$/)) {
                console.error("[Vidi] model for view '#"+id+"' defines "+
                              "invalid top-level method '"+varname+"'");
                throw new Error("Invalid model");
            }
            
            if (model[varname] !== undefined) {
                console.error("[Vidi] View '#"+id+"' defines a "+
                              "method '"+varname+"', which clashes with "+
                              "the model.");
                throw new Error("Invalid method");
            }
            
            if (typeof (def.methods[varname]) != "function") {
                console.error("[Vidi] View '#"+id+"' method '"+varname+"' "+
                              "is not a function");
                throw new Error("Invalid method");
            }
            model[varname] = def.methods[varname];
        }
        
        model.$el = null;
        
        // Set up private variables
        self.$ = self.querySelector;
        self.$id = id;
        
        self.$data = Vidi.clone (model);
        self.$el = null;
        
        if (def.watch) {
            self.$watch = def.watch
        }

        // Set up public variables
        self.view = NestProxy.create (self.$data, self);
        Vidi.views[id] = this;
        
        if (def.updated) {
            self.$onupdate = def.updated.bind(self.view);
        }
        
        if (def.created) {
            self.$oncreate = def.created.bind(self.view);
        }
        
        if (document.readyState === "complete") {
            self.init(id);
        }
        else {
            let self = this;
            window.addEventListener("load",function() {
                self.init(id);
            });
        }
    }
    
    // ------------------------------------------------------------------------
    // Binds to an actual DOM node. Gets called when the document readyState
    // is marked as complete.
    // ------------------------------------------------------------------------
    init(id) {
        let self = this;
        self.$template = document.getElementById (id);
        if (! self.$template) throw new Error("Could not bind to #"+id);
        self.$parent = self.$template.parentNode;
        // Render self
        self.render();
    }
    
    // ------------------------------------------------------------------------
    // Little shorthand for doing a query-selection on the view's head
    // element.
    // ------------------------------------------------------------------------
    querySelector(q) {
        let self = this;
        return self.$el.querySelector(q);
    }
    
    // ------------------------------------------------------------------------
    // Evals a double moustache-template value. This involves wrapping it
    // into a function that gets local references to all the model's listed
    // top-level variables, as well as an optional index and loop variable.
    // ------------------------------------------------------------------------
    eval(e,tempvars,noreturn) {
        let self = this;
        
        if (tempvars[e] !== undefined) return tempvars[e];
        if (self.$data[e] !== undefined) return self.$data[e];
        
        let src = "(function(){ return function(__self,__data,__locals) {";
        for (let id in self.$data) {
            if (e.indexOf(id)<0) continue;
            src += "var "+id+" = __data."+id;
            if (typeof (self.$data[id]) == "function") {
                src += ".bind(__self)"
            }
            src += ";"
        }
        for (let lid in tempvars) {
            if (e.indexOf(lid)<0) continue;
            src += "var "+lid+" = __locals."+lid;
            if (typeof (tempvars[lid]) == "function") {
                src += ".bind(__self)"
            }
            src += ";";
        }
        
        // Handle variable assignments
        e = e.replace(/[a-zA-Z0-9_]+ *=[^=]/g, function (m) {
            let varname=m.replace(/ *=.*/,'');
            if (self.$data[varname] !== undefined) {
                return "__data."+m;
            }
            return m;
        });
        
        // Handle postincrements.
        e = e.replace(/[a-zA-Z0-9_]+[+-]{2}/g, function (m) {
            let varname=m.replace(/[+-]{2}/,'');
            if (self.$data[varname] !== undefined) {
                return "__data."+m;
            }
            return m;
        });

        // Handle preincrements.
        e = e.replace(/[+-]{2}[a-zA-Z0-9_]+/g, function (m) {
            let varname=m.replace(/[+-]{2}/,'');
            if (self.$data[varname] !== undefined) {
                return m.replace(/[+-]{2}/,function(m) {
                    return m+"__data.";
                });
            }
            return m;
        });

        if (noreturn) src += e +";";
        else src += "return ("+e+"); ";
        src += "}})()";
        
        try {
            return eval(src)(self,self.view,tempvars);
        }
        catch (err) {
            switch (err.name) {
                case "ReferenceError":
                    Vidi.warn ("Template for '#"+self.$id+"' references "+
                               "a variable not in its model: "+err.message);
                    break;
                    
                default:
                    Vidi.warn ("Parse error in '#"+self.$id+"': "+err.message);
                    Vidi.warn ("text: "+e);
                    Vidi.warn ("expanded: "+src);
                    break;
            }
            return null;
        }
    }

    parseMoustache (str, tempvars) {
        let self = this;
        return str.replace(/{{\s?([^}]*)\s?}}/g,function(m) {
            let src = m.substr(2,m.length-4);
            let res = self.eval(src,tempvars);
            if (res === null || res === undefined) return res;
            if (typeof (res) != "object") return res;
            Vidi.warn ("[Vidi] got an object when evaluating {{"+
                       src + "}}");
            return "[Object]";
        });
    }
    
    // ------------------------------------------------------------------------
    // Renders the view out into a DOM tree
    // ------------------------------------------------------------------------
    render(tmo) {
        let self = this;
        let timeout = tmo ? tmo : 0;
        if (self.$renderlock) return;

        if (self.renderedOnce) {
            try {
                if (self.$onupdate) self.$onupdate(self);
            }
            catch (e) {
                console.warn ("[Vidi] Error in $onupdate for view '#"+
                              self.$id+"':",e);
            }
        }
        
        let dorender = function() {
            if (Vidi.debug) {
                console.log ("render");
            }
            
            let div = self.$template.cloneNode(false);
            div.setAttribute ("id", self.$id);
            div.setAttribute ("v-view", self.$id);

            for (let node of self.$template.childNodes) {
                self.renderTemplate (div, node);
            }
            
            //console.log (div.innerHTML);

            if (self.$el) {
                self.transplantDom (self.$el, div, self.renderedOnce);
            }
            else {
                self.$parent.replaceChild (div, self.$template);
                self.$el = div;
            }
            if (! self.renderedOnce) {
                if (self.$oncreate) {
                    self.$oncreate (self);
                }
                self.renderedOnce = true;
            }
            
            if (self.onRender) self.onRender();
        }
        
        if (! self.renderedOnce) dorender();
        else if (! timeout) {
            if (self.renderTimeout) {
                clearTimeout (self.renderTimeout);
            }
            delete self.renderTimeout;
            dorender();
        }
        else {
            if (self.renderTimeout) {
                clearTimeout (self.renderTimeout);
                if (Vidi.debug) console.log ("[Vidi] render postponed");
            }
            self.renderTimeout = setTimeout (function() {
                clearTimeout (self.renderTimeout);
                delete self.renderTimeout;
                dorender();
            }, timeout /* 100fps max, change to 7 for 144Hz support ;) */);
        }
    }
    
    lock() {
        let self = this;
        self.$renderlock = true;
    }
    
    unlock(norender) {
        let self = this;
        if (self.$renderlock) {
            self.$renderlock = false;
            if (! norender) self.render();
        }
    }
    
    // ------------------------------------------------------------------------
    // Set-handler for the NestProxy. We use this, rather tamely, to
    // detect changes to the view, so we can re-render the DOM tree.
    // ------------------------------------------------------------------------
    setPath (data, path, value) {
        let self = this;
        let obj = self.$data;
        let idx = 0;
        let key = null;
        
        if (Vidi.debug) {
            console.log ("setPath",data,path,value);
        }
                    
        if (path.length == 1) {
            if (self.$watch && self.$watch[path[0]]) {
                let fn = self.$watch[path[0]].bind(self.view);
                fn(value);
            }
        }
        
        for (idx=0; idx<(path.length-1); ++idx) {
            key = path[idx];
            if (obj[key] == undefined) {
                throw new Error("Can not set key '"+key+"'");
            }
            obj = obj[key];
        }
        key = path[idx];
        
        if (typeof(value) !== "object") {
            if (obj[key] === value) return true;
        }
        
        obj[key] = Vidi.clone(value);
        
        self.render(10);
        return true;
    }
    
    // ------------------------------------------------------------------------
    // Delete-handler for the NestProxy.
    // ------------------------------------------------------------------------
    deletePath (data, path) {
        let self = this;
        let obj = self.$data;
        let idx = 0;
        let key = null;
        for (idx=0; idx<(path.length-1); ++idx) {
            key = path[idx];
            if (obj[key] == undefined) {
                throw new Error("Can not set key '"+key+"'");
            }
            obj = obj[key];
        }
        key = path[idx];
        delete obj[key];
        self.render(10);
        return true;
    }
    
    // ------------------------------------------------------------------------
    // Get-handler for the NestProxy. Just stupidly forwards to the underlying
    // data in self.$data.
    // ------------------------------------------------------------------------
    getPath (data, path) {
        let self = this;
        let obj = self.$data;
        if (path[0] == "$el") {
            return self.$el;
        }

        if (path.length == 1) {
            if (path[0] == "$isView") return true;
        }
            
        for (let key of path) {
            if (typeof (obj) != "object") {
                return undefined;
            }
            obj = obj[key];
            if (! obj) return obj;
        }
        return obj;
    }
    
    // ------------------------------------------------------------------------
    // Creates a clone of an element. Note that this cannot include any
    // attached EventListeners.
    // ------------------------------------------------------------------------
    cloneElement (orig, tempvars) {
        let nw = null;
        let self = this;
        let innerhtml = null;
        let setvalue = null;
        try {
            nw = document.createElement(orig.tagName);
        }
        catch (e) {
            console.alert ("Illegal tag in orig: "+orig.tagName);
            console.log (orig);
        }
        
        
        if (orig.getAttribute) {
            let vcomp = orig.getAttribute("v-component");

            if (vcomp) {
                let component = Vidi.components[vcomp];
                if (component) {
                    tempvars = Vidi.copy (tempvars);
                    tempvars["$component"] = component.functions;
                    let vinstance = orig.getAttribute ("v-instance");
                    if (vinstance) {
                        let instance = Vidi.instances[vcomp][vinstance];
                        if (! instance.view) {
                            instance.view = self.view;
                            instance.render = function() { self.render(); };
                            if (instance.functions &&
                                instance.functions.construct) {
                                instance.functions.construct(instance);
                            }
                        }
                        tempvars["$instance"] = instance;
                    }
                }
            }
        }
        
        // We're cloning out of a template, so we'll need to handle
        // the attributes where needed, and copy the rest.
        if (orig.getAttributeNames) {
            let checksumstr = ""; // collect checksum for all code found in
                                  // v-on: attributes to generate a checksum
                                  // id. This allows us to distinguish nodes
                                  // with unique event listeners.
            for (let a of orig.getAttributeNames()) {
                let val = orig.getAttribute (a);
                if (a.startsWith(":")) a = "v-bind"+a;
                // Vidi-specific attributes
                if (a.startsWith("v-")) {
                    // Handle the v-model property
                    let attrbase = a.split(':')[0];
                    switch (attrbase) {
                    
                        // The v-model attribute: Tracks changes to an
                        // input element's value and reflects them back
                        // to the view.
                        case "v-model":
                            let tvars = tempvars;
                            let model = val;
                            let curval = self.getChild(tempvars, model);
                            let inputtype = null;
                            let tagName = orig.tagName;
                            setvalue = curval;
                            let evid = "input";
                            if (tagName == "SELECT") {
                                evid = "change";
                            }
                            if (tagName == "INPUT") {
                                inputtype = orig.getAttribute("type");
                                if (inputtype) {
                                    inputtype = inputtype.toLowerCase();
                                }
                                if (inputtype == "checkbox" ||
                                    inputtype == "radio") {
                                    evid = "change";
                                    if (inputtype == "radio") {
                                        setvalue = orig.value;
                                        if (setvalue == curval) {
                                            nw.setAttribute ("checked","");
                                            nw.checked = true;
                                        }
                                        else {
                                            nw.checked = false;
                                        }
                                        nw.setAttribute("u-debug",Vidi.uuidv4());
                                    }
                                    else {
                                        if (setvalue) {
                                            nw.checked = true;
                                            nw.setAttribute ("checked","");
                                            nw.value = "on";
                                        }
                                        else {
                                            nw.checked = false;
                                            nw.value = "off";
                                        }
                                    }
                                }
                                else if (inputtype == "number") {
                                    if (!curval) {
                                        curval = 0;
                                        nw.value = "0";
                                    }
                                }
                            }
                            nw.addEventListener(evid, function() {
                                let curval = self.getChild(tvars, model);
                                let newval = nw.value;
                                if (inputtype == "checkbox") {
                                    newval = nw.checked;
                                }
                                else if (inputtype == "radio") {
                                    newval = this.checked ?
                                             this.value :
                                             curval;
                                }

                                if (newval != curval) {
                                    //self.lock();
                                    this.setAttribute ("v-edited",1);
                                    self.setChild(tvars, model, newval);
                                    self.render(10);
                                    //self.unlock(true);
                                    //self.render(50);
                                }
                            });
                            checksumstr += "//"+val;
                            break;
                        
                        // The v-on attribute: Sets up an event handler
                        // on an element.
                        case "v-on":
                            let evname = a.substr(5);
                            
                            let ontv = tempvars;
                            if (val.indexOf('{{') >= 0) {
                                val = self.parseMoustache (val, tempvars);
                            }

                            checksumstr += "//" + val;
                            nw.addEventListener(evname, function(e) {
                                ontv["$event"] = e;
                                let r = self.eval(val, ontv, true);
                                if (typeof (r) == "function") {
                                    r(e);
                                }
                            });
                            break;
                         
                        // The v-bind attribute allows attribute-values
                        // to be programmatic from the view without needing
                        // to enmoustache their arguments, making setting
                        // up bindings from within a component less awkward.   
                        case "v-bind":
                            let bindto = a.substr(7);
                            
                            if (val.indexOf('{{') >= 0) {
                                val = self.parseMoustache (val, tempvars);
                            }
                            
                            if (val.startsWith('{') &&
                                     val.endsWith('}')) {
                                let res = self.eval(val, tempvars);
                                let resarray = [];
                                for (let k in res) {
                                    if (res[k]) resarray.push(k);
                                }
                                val = resarray.join(' ');
                            }
                            else {
                                val = self.eval (val, tempvars);
                            }
                            if (val !== false && val !== undefined) {
                                if (val === true) {
                                    nw.setAttribute (bindto, "");
                                }
                                else {
                                    if (orig.getAttribute (bindto)) {
                                        val = orig.getAttribute (bindto) +
                                              " " + val;
                                    }
                                    nw.setAttribute (bindto, val);
                                }
                            }
                            else if (val === false) {
                                nw.removeAttribute (bindto);
                            }
                            break;
                            
                        case "v-html":
                            innerhtml = self.eval (val, tempvars);
                            break;
                        
                        // The v-component attribute tracks the head node
                        // of a component. Keep it around because it makes
                        // debugging the element tree much simpler.    
                        case "v-component":
                        case "v-static":
                            nw.setAttribute (a, val);
                            break;
                            
                        case "v-if":
                        case "v-for":
                        case "v-eventid":
                        case "v-show":
                        case "v-instance":
                            break;
                            
                        default:
                            Vidi.warn ("Unknown v-attribute: "+a);
                            break;
                    }
                    
                    // Don't copy any other Vidi attribute into the
                    // rendered DOM tree, it's only of internal use.
                    continue;
                }
                
                if (val.indexOf("{{")>=0 &&
                    val.indexOf("}}")>=0) {
                    
                    val = self.parseMoustache (val, tempvars);
                }
                if (! orig.getAttribute ("v-bind:"+a)) {
                    nw.setAttribute (a, val);
                }
            }
            
            // If there's data in the checksumstr, that means we added
            // event listeners to the node, so we need to make sure
            // that any DOM-reorganizing fuckery happening in the view
            // will distinguish nodes that have the exact same attributes
            // but are bound to different listeners.
            if (checksumstr.length) {
                let tv = {};
                for (let key in tempvars) {
                    if (! key.startsWith('$')) {
                        tv[key] = tempvars[key];
                    }
                }
                checksumstr += "//" + JSON.stringify(tv);
                let checksum = Vidi.checksum(checksumstr);
                if (Vidi.debug) {
                    console.log ("[Vidi] event-id "+checksum+": "+checksumstr);
                }
                nw.setAttribute ("v-eventid", checksum);
            }
        }
        
        if (innerhtml) {
            nw.innerHTML = innerhtml;
            return nw;
        }
        
        // Render any children
        for (let cn of orig.childNodes) {
            self.renderTemplate (nw, cn, tempvars);
        }
        
        // If we're a text-node, do things to our textContent.
        if (! orig.childElementCount) {
            let text = orig.textContent;
            if (text) {
                text = self.parseMoustache (text, tempvars);
                nw.textContent = text;
            }
        }
        
        if (setvalue) nw.value = setvalue;
        return nw;
    }
    
    // ------------------------------------------------------------------------
    // Clones a DOM element (and its children) and adds it as a child to
    // the target element. Attributes and text-data belonging to the element
    // and its children are parsed for moustache-templates.
    // ------------------------------------------------------------------------
    appendClone (into, orig, tempvars, hide) {
        let self = this;
        let nw = self.cloneElement (orig, tempvars);
        if (hide) nw.style.display = "none";
        into.appendChild (nw);
        return nw;
    }
    
    // ------------------------------------------------------------------------
    // Get a child node of an object by an index with a possible dot, i.e
    // getChild ({foo:{bar:42}}, "foo.bar") returns 42.
    // ------------------------------------------------------------------------
    getChild (obj, index) {
        let self = this;
        let get = function (obj, index) {
            if (index.indexOf('.')<0) return obj[index];
            let keys = index.split('.');
            let crsr = obj;
            for (let key of keys) {
                crsr = crsr[key];
                if (! crsr) {
                    return crsr;
                }
            }
            return crsr;
        }
        
        let res = get (obj,index);
        if (res === undefined) {
            return get (self.view, index);
        }
        return res;
    }
    
    setChild (obj, index, value) {
        let self = this;
        if (index.indexOf('.')<0) {
            if (obj[index] === undefined) {
                self.view[index] = value;
            }
            else {
                obj[index] = value;
            }
            return;
        }
        
        let keys = index.split('.');
        let crsr = obj;
        
        if (crsr[keys[0]] === undefined) {
            crsr = self.view;
        }

        for (let i=0; i<keys.length-1; ++i) {
            crsr = crsr[keys[i]];
            if (crsr === undefined) {
                Vidi.warn ("Could not update "+index);
                return;
            }
        }
        crsr[keys.slice(-1)[0]] = value;
    }
    
    // ------------------------------------------------------------------------
    // Compares two DOM nodes, sans children. If it's possible to do a
    // gentle mutation, like swapping in the right node's attributes,
    // or input-value, goes ahead and does that. Otherwise, returns false.
    // ------------------------------------------------------------------------
    compareNodes (left, right) {
        if (left.nodeType != right.nodeType) {
            return false;
        }
        if (left.tagName != right.tagName) {
            return false;
        }
        if (left.checked != right.checked) {
            left.checked = right.checked;
        }

        if (left.getAttribute) {
            let vstatic = left.getAttribute ("v-static");
            if (vstatic == "keep") return true;
        }
        
        let ll = left.childNodes.length;
        let rl = right.childNodes.length;
        
        if (ll < rl) {
            if (! ll) return false;
            if (ll > 100) return false;
            while (left.childNodes.length < rl) {
                left.appendChild(document.createElement("div"));
            }
        }
        else if (ll > rl) {
            if (ll > 100) return false;
            while (left.childNodes.length > rl) {
                left.removeChild(left.lastChild);
            }
        }

        if (! left.childElementCount) {
            if (left.textContent != right.textContent) return false;
        }
        
        if (left.value != right.value) {
            if (left.getAttribute ("v-edited")) {
                left.removeAttribute ("v-edited");
            }
            else {
                left.value = right.value;
            }
        }
        
        if (left.nodeType == Node.ELEMENT_NODE) {
            // In case of bound events, we should consider the elements
            // un-swappable. Other attributes can be safely transplanted
            // if the element is unbound.
            let lefteventid = left.getAttribute("v-eventid");
            let righteventid = right.getAttribute("v-eventid");
            
            if (lefteventid||righteventid) {
                if (lefteventid != righteventid) return false;
            }
            
            for (let a of left.getAttributeNames()) {
                if (left.getAttribute(a) !== right.getAttribute(a)) {
                   if (right.getAttribute(a) === null) {
                        left.removeAttribute(a);
                    }
                    else {
                        left.setAttribute(a, right.getAttribute(a));
                    }
                }
            }
            for (let a of right.getAttributeNames()) {
                if (left.getAttribute(a) !== right.getAttribute(a)) {
                    if (right.getAttribute(a) === null) {
                        left.removeAttribute(a);
                    }
                    else {
                        left.setAttribute(a, right.getAttribute(a));
                    }
                }
            }
        }

        return true;
    }
    
    // ------------------------------------------------------------------------
    // Transforms a target DOM node to match the output of a new render,
    // keeping subtrees that are unchanged.
    // ------------------------------------------------------------------------
    transplantDom (into, from, isroot) {
        let self = this;
        
        if (! isroot) {
            if (into.getAttribute) {
                let vstatic = into.getAttribute("v-static");
                if (vstatic == "keep") return;
                if (vstatic == "non-interactive") {
                    into.parentNode.replaceChild (from, into);
                    return;
                }
            }
            
            if (! self.compareNodes (into, from)) {
                let hasfocus = false;
                if (into == document.activeElement) {
                    if (into.tagName == "INPUT" || into.tagName == "TEXTAREA") {
                        if (into.value != from.value) {
                            into.value = from.value;
                        }
                        return;
                    }
                    hasfocus = true;
                }
                if (Vidi.debug) {
                    console.log ("[Vidi] replace: ",into);
                    console.log ("[Vidi] with: ",from);
                }
                if (from.parentNode) {
                    // We swap out the original for an inferior clone, so we
                    // can keep its event listeners, but don't mess up the
                    // number of elements in the original's parent while we're
                    // iterating over them, which is considered bad form.
                    let sup = self.cloneElement (from, {});
                    from.parentNode.replaceChild (sup, from);
                }
                into.parentNode.replaceChild (from, into);
                if (hasfocus) from.focus();
                return;
            }
        }
        
        for (let i=0; i<from.childNodes.length; ++i) {
            self.transplantDom (into.childNodes[i], from.childNodes[i]);
        }
    }
    
    // ------------------------------------------------------------------------
    // Renders out a specific node within the DOM tree of the template.
    // Handles the v-for and v-if attributes, as well as moustache
    // template values inside text nodes.
    // ------------------------------------------------------------------------
    renderTemplate (into, orig, tempvars) {
        let self = this;
        if (orig === undefined) throw new Error ("orig not set");
        if (! tempvars) tempvars = {};

        if (orig.nodeType == Node.ELEMENT_NODE) {
            
            // Handle the v-if attribute
            let vif = orig.getAttribute ("v-if");
            if (vif) {
                if (! self.eval (vif, tempvars)) {
                    // Leave a comment in place of the node, making
                    // sure the node's parent keeps the same number
                    // of child elements.
                    into.appendChild (document.createComment ("v-if"));
                    return;
                }
            }
            
            let hide = false;
            let vshow = orig.getAttribute ("v-show");
            if (vshow) {
                if (! self.eval (vshow, tempvars)) {
                    hide = true;
                }
            }
            
            // Handle the v-for attribute
            let loopfor = orig.getAttribute ("v-for");
            if (loopfor) {
                let split = loopfor.split(" in ");
                let loopval = split[1];
                if (split[0].startsWith('(')) {
                    split[0] = split[0].substr(1,split[0].length-2);
                }
                let rsplit = split[0].split(",");
                let indexvar = null;
                let loopvar = rsplit[0];
                let countvar = null;
                
                if (rsplit.length > 1) {
                    indexvar = rsplit[1];
                }
                
                if (rsplit.length > 2) {
                    countvar = rsplit[2];
                }
                
                let data;
                if (!isNaN(parseInt(loopval))) {
                    let nval = Array.apply (null, {length:loopval})
                                    .map (Number.call, Number);
                    data = nval;
                }
                else {
                    data = self.eval(loopval, tempvars);
                }
                let index=0;
                for (let i in data) {
                    if (data[i] === undefined) continue;
                    let tv = Vidi.copy(tempvars);
                    if (indexvar) tv[indexvar] = i;
                    if (countvar) tv[countvar] = index++;
                    
                    if (typeof (data[i]) == "object") {
                        tv[loopvar] = data[i];
                    }
                    else {
                        tv[loopvar] = new Proxy ({}, {
                            get: function (target, key) {
                                if (key == "__viewproxy") return true;
                                if (key == "value") return data[i];
                                return undefined;
                            },
                            set: function (target, key, val) {
                                if (key == "value") {
                                    data[i] = val;
                                    return true;
                                }
                                return false;
                            }
                        });
                    }
                        
                    let elm = self.appendClone (into, orig, tv, hide);
                }
            }
            else {
                self.appendClone (into, orig, tempvars, hide);
            }
        }
        else if (orig.nodeType == Node.TEXT_NODE) {
            let text = orig.textContent;
            if (text) {
                try {
                    text = self.parseMoustache (text, tempvars);
                }
                catch (e) {
                    Vidi.warn("replacetext: "+text);
                    console.log (tempvars);
                }
                let nw = document.createTextNode(text);
                into.appendChild (nw);
            }
        }
    }
}

// ============================================================================
// CLASS VidiComponent
// -------------------
// Implements a custom HTML tag. Each instance of the defined tag in the DOM
// will be replaced with a template.
// ============================================================================
class VidiComponent {
    constructor (name, def) {
        this.$name = name;
        this.$def = def;
        this.$template = def.template.replace(/^[ \n]*/, "");
        this.functions = def.functions;
        this.$def.attributes["v-if"] = Vidi.Attribute.COPY;
        this.$def.attributes["v-for"] = Vidi.Attribute.COPY;
        
        let self = this;
        
        Vidi.components[name] = {
            "$name": self.$name,
            "$def": self.$def,
            "$template": self.$template,
            "functions": self.functions,
            "view": null,
            "renderElement":function(root,ctx) {
                return self.renderElement(root,ctx);
            },
            "render": function() {
                throw new Error("render() called on unbound instance");
            }
        }
    }
    
    // ------------------------------------------------------------------------
    // Renders out all instances of the custom component in the DOM tree.
    // ------------------------------------------------------------------------
    render(rootnode) {
        let self = this;
        let root = rootnode ? rootnode : document;
        
        let elements = root.getElementsByTagName (self.$name);
        for (let i = elements.length-1; i>=0; --i) {
            self.renderElement (elements[i]);
        }
    }
    
    // ------------------------------------------------------------------------
    // Render out a top level element
    // ------------------------------------------------------------------------
    renderElement(elm,context) {
        let self = this;
        let def = self.$def;
        
        var attr = {};
        for (let a in def.attributes) {
            let inattr = elm.getAttribute (a);
            let defattr = def.attributes[a];
            if (defattr == Vidi.Attribute.REQUIRED && !inattr) {
                console.warn ("[Vidi] Found <"+self.$name+"> element with "+
                              "missing required attribute '"+attr+"'",
                              elm);
            }
            
            if (defattr == Vidi.Attribute.IMPORT && !inattr) {
                if (context.exports[a]) attr[a] = context.exports[a];
            }
            
            if (defattr == Vidi.Attribute.IMPORTREQUIRED && !inattr) {
                if (! context.exports[a]) {
                    console.warn ("[Vidi] Found <"+self.$name+"> element with "+
                                  "missing required attribute '"+attr+"' "+
                                  "either directly or from a parent-component",
                                  elm);
                }
                else {
                    attr[a] = context.exports[a];
                }
            }
            
            if (inattr) {
                if (defattr != Vidi.Attribute.STRIP) {
                    attr[a] = inattr;
                }
            }
        }
        
        let children = { $all:[] };
        if (def.children) {
            for (let tchild of elm.childNodes) {
                if (! tchild.getAttribute) continue;
                let childtype = tchild.tagName.toLowerCase();
                if (def.children[childtype] === undefined) {
                    throw new Error("Child of type <"+childtype+"> is not "+
                                    "defined for component <"+self.$name+">");
                }
                let newchild = { type:childtype, attr:{}, innerhtml:"" };
                for (let cattr in def.children[childtype]) {
                    let attr = tchild.getAttribute (cattr);
                    if (! attr) {
                        let c = def.children[childtype][cattr];
                        if (c == Vidi.Attribute.REQUIRED) {
                            Vidi.warn ("Instance of component <"+self.$name+
                                       "> missing required attribute '"+
                                       cattr+"' for child element of type "+
                                       "<"+childtype+">");
                        }
                    }
                    else {
                        newchild.attr[cattr] = attr;
                    }
                }
                newchild.innerhtml = tchild.innerHTML;
                if (! children[childtype]) children[childtype] = [];
                children[childtype].push (newchild);
                children.$all.push (newchild);
            }
            elm.innerHTML = "";
        }
        
        let div = document.createElement ("div");
        let inner = elm.innerHTML;
        div.innerHTML = self.$template;
        let ctx = Vidi.copy(context);
        self.fixAttributes (div, attr, def.functions, inner, ctx, children);
        context.exports = ctx.exports;
        
        for (let a in def.attributes) {
            let defattr = def.attributes[a];
            if (defattr == Vidi.Attribute.COPY) {
                if (elm.getAttribute(a)) {
                    div.firstChild.setAttribute(a, elm.getAttribute(a));
                }
            }
            else if (defattr == Vidi.Attribute.EXPORT) {
                if (elm.getAttribute(a)) {
                    if (! context.exports) context.exports = {};
                    context.exports[a] = elm.getAttribute(a);
                }
                else {
                    if (! context.exports) context.exports = {};
                    context.exports[a] = "";
                }
            }
        }
        
        let instanceid = 0;
        let elmid = elm.getAttribute("id");
        instanceid = Vidi.uuidv4();

        div.firstChild.setAttribute("v-component", self.$name);
        div.firstChild.setAttribute("v-instance", instanceid);
        if (! Vidi.instances[self.$name]) {
            Vidi.instances[self.$name] = {};
        }
        
        Vidi.instances[self.$name][instanceid] = { 
            attr: attr,
            children: children,
            state: {},
            functions: self.functions,
            exports: context.exports,
            $el: div.firstChild
        };
        
        let firstElement = div.childNodes[0];

        for (let i=0; i<div.childNodes.length; ++i) {
            let node = div.childNodes[i];
            
            if (elm.nextSibling) {
                elm.parentNode.insertBefore(node,elm.nextSibling);
            }
            else {
                elm.parentNode.appendChild(node);
            }
        }
        
        elm.parentNode.removeChild (elm);
        return firstElement;
    }
    
    // ------------------------------------------------------------------------
    // Expands any code in attributes, and inner text, handles the
    // 'component-for' attribute, and removes attributes that have no data 
    // after doing the above.
    // ------------------------------------------------------------------------
    fixAttributes (node, attr, funcs, inner, tempvars, children, top) {
        let self = this;
        
        if (node.getAttributeNames) {
            for (let a of node.getAttributeNames()) {
                let val = node.getAttribute(a);
                if (a == "component-if") {
                    if (! self.eval (val, attr, funcs, inner, 
                                     tempvars, children)) {
                        node.parentNode.removeChild (node);
                        return;
                    }
                    node.removeAttribute (a);
                }
                else if (a == "component-for") {
                    node.removeAttribute (a);
                    let split = val.split(" in ");
                    let splitvar = split[0];
                    let splitindex = null;
                    let tv = Vidi.copy(tempvars);
                    
                    if (splitvar.indexOf(",") > 0) {
                        let tuple = splitvar.split(",");
                        splitvar = tuple[0];
                        splitindex = tuple[1];
                    }
                    
                    let iter = self.eval (split[1], attr, funcs, inner,
                                          tempvars, children);

                    let nextSibling = node.nextSibling;
                    let parentNode = node.parentNode;
                    
                    for (let obj of iter) {
                        tv[splitvar] = obj;
                        if (splitindex) tv[splitindex] = obj;
                        let nw = node.cloneNode(true);
                        
                        self.fixAttributes (nw, attr, funcs, inner,
                                            tv, children);
                        
                        tempvars.exports = tv.exports;
                        
                        if (nextSibling) {
                            parentNode.insertBefore(nw,nextSibling);
                        }
                        else {
                            parentNode.appendChild(nw);
                        }
                    }
                    parentNode.removeChild(node);
                    return;
                }
                else {
                    if (val.indexOf("{{") >= 0) {
                        val = val.replace(/{{\s?([^}]*)\s?}}/g,function(m) {
                            let src = m.substr(2,m.length-4);
                            let res = self.eval(src, attr, funcs, inner,
                                                tempvars, children);
                            if (res === undefined || res === null) return "";
                            if (res === true) val = true;
                            return res;
                        });
                    }

                    if (val === true) {
                        node.setAttribute (a, "");
                    }
                    else if (! val) {
                        node.removeAttribute (a);
                    }
                    else node.setAttribute (a, val);
                }
            }
        }
        
        if (! node.childElementCount) {
            let text = "";
            
            if (node.nodeType == Node.TEXT_NODE) {
                text = node.textContent;
            }
            else {
                text = node.innerHTML;
            }

            if (text) {
                text = text.replace(/{{\s?([^}]*)\s?}}/g,function(m) {
                    let src = m.substr(2,m.length-4);
                    return self.eval(src, attr, funcs, inner,
                                     tempvars, children);
                });
                
                node.innerHTML = text;
            }
            return;
        }
        
        // fixAttributes can mess with our child list through loops,
        // so get a list of children to fix in a separate loop.
        let fixlist = [];
        for (let i=0; i<node.childNodes.length; ++i) {
            if (node.childNodes[i].nodeType == Node.TEXT_NODE) continue;
            if (node.childNodes[i].getAttribute("v-component")) continue;
            fixlist.push (node.childNodes[i]);
        }
        
        for (let child of fixlist) {
            self.fixAttributes (child, attr, funcs,
                                inner, tempvars, children);
        }
        
    }
    
    // ------------------------------------------------------------------------
    // Execute a render function defined for the template.
    // ------------------------------------------------------------------------
    eval (txt, attr, func, inner, tempvars, children, context) {
        let self = this;
        let src = "(function(){ return "+
                  "function(attr,func,innerhtml,children,context,__tmp) {";
        for (let fid in func) {
            src += "var "+fid+" = func."+fid+";";
        }
        if (tempvars) for (let tvid in tempvars) {
            src += "var "+tvid+" = __tmp."+tvid+";";
        }
        
        src += "return ("+txt+"); ";
        src += "}})()";
        
        try {
            return eval(src)(attr,func,inner,children,context,tempvars);
        }
        catch (e) {
            Vidi.warn ("Parse error in component '"+self.name+"': "+e);
            Vidi.warn ("text: "+src);
            return null;
        }
    }
}

// ============================================================================
// BASE OBJECT
// ============================================================================
Vidi = {
    View:VidiView,
    Component:VidiComponent,
    Attribute:{
        OPTIONAL: 1,
        REQUIRED: 2,
        COPY: 3,
        STRIP: 4,
        EXPORT: 5,
        IMPORT: 6,
        IMPORTREQUIRED: 7
    },
    instances:{},
    components:{},
    views:{},
    $waiting:{}
}

// ============================================================================
// Copy-by-value helper function.
// ============================================================================
Vidi.clone = function(obj) {
    if (null == obj || "object" != typeof obj) return obj;
    var copy;
    if (typeof (obj) == "function") return obj;
    if (typeof(obj.constructor) == "function") {
        try {
            copy = obj.constructor();
        }
        catch (e) {
            console.warn ("[Vidi] funny:",e);
            copy = {}
        }
    }
    else {
        copy = {};
    }
    for (var attr in obj) {
        if (obj.hasOwnProperty && obj.hasOwnProperty(attr)) {
            if (typeof obj[attr] == "object") {
                copy[attr] = Vidi.clone(obj[attr]);
            }
            else copy[attr] = obj[attr];
        }
        else {
            if (! obj.hasOwnProperty) copy[attr] = obj[attr];
        }
    }
    return copy;
}

// ============================================================================
// Create a shallow clone of an object, keeping the member variables
// as references, but allowing mutations to the actual member list
// separated from the original.
// ============================================================================
Vidi.copy = function(obj) {
    if (null == obj || "object" != typeof obj) return obj;
    if (typeof (obj) == "function") return obj;
    let locals = {};
    return new Proxy(obj, {
        set: function (o,prop,value) {
            if (o[prop] !== undefined && o[prop] !== null) o[prop] = value;
            else {
                if (locals[prop] !== undefined &&
                    locals[prop] !== null &&
                    typeof (locals[prop]) == "object") {
                    if (locals[prop].__viewproxy) {
                        locals[prop].value = value;
                        return true;
                    }
                }
                locals[prop] = value;
                o[prop] = null;
            }
            return true;
        },
        get: function (o,prop) {
            if (o[prop] !== undefined && o[prop] !== null) return o[prop];
            if (locals[prop] !== undefined &&
                locals[prop] !== null &&
                typeof (locals[prop]) == "object") {
                if (locals[prop].__viewproxy) return locals[prop].value;
            }
            return locals[prop];
        }
    });
    
    var copy = {};
    for (var attr in obj) {
        copy[attr] = obj[attr];
    }
    return copy;
}

// ============================================================================
// Generate a uuid (used for tracking component instances)
// ============================================================================
Vidi.uuidv4 = function() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'
           .replace(/[xy]/g, function(c) {
        var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8);
        return v.toString(16);
    });
}

Vidi.accessKey = function(obj, key) {
    if (key.indexOf('.')<0) return obj[key];
    let crsr = obj;
    let keys = key.split('.');
    for (let k of keys) {
        crsr = crsr[k];
        if (! crsr) return crsr;
    }
    return crsr;
}

// ============================================================================
// Generate simple hash for a string. Used when setting up the
// EventListeners for a DOM node, to keep track of unique combinations of
// event handling code fragments and closure data.
// ============================================================================
Vidi.checksum = function(str) {
    let s = str;
    if (typeof (s) == "object") s = JSON.stringify(s);
    if (typeof (s) != "string") s = String(s);
    let hash=0;
    for (let i=0; i < s.length; i++) {
      let chr = s.charCodeAt(i);
      hash = ((hash << 5) - hash) + chr;
      hash |= 0;
    }
    if (hash<0) hash = -(hash+1);
    let code = hash.toString(16).padStart(8,'0');
    return code.substr(0,4) + "-" + code.substr(4);
}

Vidi.renderComponents = function(root, context) {
    let crsr = root;
    if (! crsr) crsr = document;
    if (! context) context = {};
    
    if (crsr.tagName) {
        let id = crsr.tagName.toLowerCase();
        if (crsr.getAttribute ("v-instance")) return;
        if (Vidi.components[id] !== undefined) {
            crsr = Vidi.components[id].renderElement (crsr, context);
        }
    }
    if (! crsr.childNodes) return;
    
    let children = [];
    for (let ch of crsr.childNodes) {
        Vidi.renderComponents (ch, context);
    }
}

// ============================================================================
// Log helper
// ============================================================================
Vidi.warn = function(str) {
    console.warn ("[Vidi] "+str);
}

if (document.readyState === "complete") {
    Vidi.renderComponents (document);
}
else {
    let self = this;
    window.addEventListener("load",function() {
        Vidi.renderComponents (document);
    });
}
