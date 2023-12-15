// ==========================================================================
// Code for augmenting the HTML markup of modules, prior to Vueifying their
// viewport.
// ==========================================================================
MarkupDecorators = {}
Markup = {}

// --------------------------------------------------------------------------
// Go over the markup for a module for each decorator tag registered in
// MarkupDecorators.
// --------------------------------------------------------------------------
Markup.decorate = function (moduleid) {
    for (k in MarkupDecorators) {
        var sel = '#' + moduleid + " u-" + k;
        var query = $(sel);

        Markup.iterate (query, function(e) {
            MarkupDecorators[k](e, moduleid);
        });
        query.remove();
    }
}

// --------------------------------------------------------------------------
// Utility function for scrolling to an element as needed.
// --------------------------------------------------------------------------
Markup.lazyScroll = function(element,parent){
    var offset = $(element).offset().top - $(parent).offset().top;
    var eh = $(element).height();
    var ph = $(parent).height();

    if (offset < 0) {
        var scrolltop = $(parent).scrollTop();
        $(parent).animate({scrollTop:scrolltop+offset},50);
        return;
    }
    offset = $(element).offset().top + eh;
    offset -= $(parent).offset().top + ph;
    if (offset > 0) {
        var scrolltop = $(parent).scrollTop();
        $(parent).animate({scrollTop:scrolltop+offset},50);
    }
}

// --------------------------------------------------------------------------
// Simple iterator over a decorator's matches.
// --------------------------------------------------------------------------
Markup.iterate = function (q, fn) {
    for (var i=0; i<q.length; ++i) {
        fn (q[i]);
    }
}

// --------------------------------------------------------------------------
// Find a specified attribute in any of the nodes up the tree from the first
// element of a jquery.
// --------------------------------------------------------------------------
Markup.getParentAttribute = function (e, attrname, suffix) {
    var pnode = e.parentElement;
    while (pnode && !pnode.hasAttribute (attrname)) {
        pnode = pnode.parentElement;
    }
    if (pnode) {
        if (suffix) return pnode.getAttribute(attrname) + suffix;
        return pnode.getAttribute (attrname);
    }
    return "";
}

// --------------------------------------------------------------------------
// Implementation of the <u-tabgroup> tag.
// --------------------------------------------------------------------------
MarkupDecorators.tabgroup = function (e, mname) {
    var tabstr = e.getAttribute ("tabs");
    var name = e.getAttribute ("name");
    var extraclass = e.getAttribute ("class");
    var tabs = tabstr.split(',');
    var cb = e.getAttribute ("cb");
    var attrbadges = e.getAttribute ("badges");
    var pbadge = e.getAttribute ("parent-badge");
    var badges = [];
    if (attrbadges) badges = attrbadges.split(',');
    
    for (var ti=tabs.length-1; ti>=0; --ti) {
        var tabid = tabs[ti].split(':')[0];
        var tablabel = tabs[ti].split(':')[1];
        if (! tablabel) tablabel=tabid;
        var classsuffix = badges[ti]!==undefined ? " badged" : "";
        if (extraclass) classsuffix += " " + extraclass;

        var ubut = document.createElement ("u-button");
        if (ti==0) {
            ubut.className = "tabgroup left"+classsuffix;
        }
        else if ((ti+1)<tabs.length) {
            ubut.className = "tabgroup middle"+classsuffix;
        }
        else {
            ubut.className = "tabgroup right"+classsuffix;
        }
        if (badges[ti]) ubut.setAttribute ("badge", badges[ti]);
        if (pbadge) ubut.setAttribute ("parent-badge", pbadge);
        if (cb) {
                ubut.setAttribute ("cb",cb + "('"+tabid+"')");
        }
        else ubut.setAttribute ("cb",name+"='"+tabid+"'");
        ubut.setAttribute ("bindclass","{active:"+name+"=='"+tabid+"'}");
        ubut.innerHTML = tablabel;
        $(ubut).insertAfter (e);
        MarkupDecorators.button (ubut, mname);
        $(ubut).remove();
    }
}

// --------------------------------------------------------------------------
// Decorator for the <u-menuitem> tag used in the side bar.
// --------------------------------------------------------------------------
MarkupDecorators.menuitem = function (e, mname) {
    var icon = e.getAttribute ("icon");
    var path = e.getAttribute ("path");
    var route = e.getAttribute ("route");
    var click = e.getAttribute ("onclick");
    var role = e.getAttribute ("role");
    var module = e.getAttribute ("module");
    var badgeId = e.getAttribute ("badge");
    var parentBadge = e.getAttribute("parent-badge");

    var ediv = document.createElement("div");
    ediv.className = "uMenuItem";
    if (click) ediv.setAttribute ("onclick",click);
    else if (route) {
        ediv.setAttribute ("onclick","App.goDeep('"+route+"')");
        ediv.setAttribute ("u-route", route);
    }
    if (role) ediv.setAttribute ("u-role", role);
    if (module) ediv.setAttribute ("u-module", module);

    if (icon) {
        var eimg = document.createElement("img");
        eimg.src = "icon/"+icon;
        ediv.appendChild (eimg);
    }

    var espan = document.createElement("span");
    espan.innerHTML = e.innerHTML;
    ediv.appendChild (espan);
    
    if (badgeId) {
        ediv.setAttribute ("u-badge", badgeId);
        ediv.setAttribute ("u-badge-mounted",1);
        if (parentBadge) ediv.setAttribute ("u-badge-parent", parentBadge);
        var bdiv = document.createElement("div");
        bdiv.setAttribute ("class","badge");
        bdiv.innerHTML = "0";
        ediv.appendChild (bdiv);
        
        ediv.addEventListener ("increaseBadge", function(ev) {
            let oldcount = parseInt(bdiv.innerHTML);
            let count = ev.count;
            if (! count) count = 1;
            bdiv.innerHTML = oldcount+count;
            if (oldcount+count == 0) {
                if (oldcount) bdiv.classList.remove ("nonzero");
            }
            else {
                if (!oldcount) bdiv.classList.add ("nonzero");
            }
        });
        
        ediv.addEventListener ("setBadge", function(ev) {
            var oldcount = parseInt (bdiv.innerHTML);
            bdiv.innerHTML = ev.count;
            if (! ev.count) bdiv.classList.remove ("nonzero");
            else if (! oldcount) bdiv.classList.add ("nonzero");
        });
        
        ediv.addEventListener ("clearBadge", function() {
            bdiv.innerHTML = "0";
            bdiv.classList.remove ("nonzero");
        });
    }

    $(ediv).insertAfter(e);
}

// --------------------------------------------------------------------------
// Encloses a page in a Wizard-style dialog. Takes care of page navigation:
// - Disables the back-button if it's on the first page
// - Disables the next-button if the wizard module's validateAll() gives
//   a negative response.
// - Turns the next-button into a finish-button on the last page.
// The Module.Wizard base object handles further callbacks to the module.
// --------------------------------------------------------------------------
MarkupDecorators.wizardpage = function (e, mname) {
    var page = e.getAttribute ("page");
    var ebutton = e.getAttribute ("extrabutton");
    var ecb = e.getAttribute ("extracb");
    var ereq = e.getAttribute ("extrarequires");
    var tfinish = e.getAttribute ("finish");
    var oncancel = e.getAttribute ("oncancel");
    if (! oncancel) oncancel="hide()";

    if (! tfinish) tfinish = "Finish";

    var elm = document.createElement ("span");
    elm.setAttribute ("v-if","page=="+page);
    elm.innerHTML = e.innerHTML;

    var bdiv = document.createElement ("div");
    bdiv.className = "uWizardButtons";
    //bdiv.setAttribute ("v-on:click","showPage(page)");

    var bt0 = document.createElement ("button");
    bt0.setAttribute("v-on:click",oncancel);
    bt0.setAttribute("v-bind:class",
                     "{backbutton:page<2}");
    bt0.innerHTML = "Cancel";

    var bte;
    if (ebutton) {
        bte = document.createElement ("button");
        bte.style.cssFloat = "left";
        bte.className = "backbutton";
        bte.innerHTML = ebutton;
        bte.setAttribute ("v-on:click",ecb);
        if (ereq) {
            bte.setAttribute ("v-bind:disabled","!"+req);
        }
    }

    var bt1 = document.createElement ("button");
    bt1.style.cssFloat = "left";
    bt1.setAttribute("v-bind:disabled","page<2");
    bt1.setAttribute("v-on:click","previousPage($event)");
    bt1.setAttribute("v-bind:class",
                     "{backbutton:page>1}");
    bt1.setAttribute ("v-if","pagecount>1");
    bt1.className = "green withicon";
    bt1.innerHTML = "Previous";
    bt1.style.backgroundImage = "url('icon/toolbar-back.png')";

    var bt2 = document.createElement ("button");
    bt2.className="green enteraction";
    bt2.setAttribute("v-bind:disabled","!validateAll()");
    bt2.setAttribute("v-on:click","nextPage($event)");
    bt2.innerText = "{{(page<pagecount)?'Next':'"+tfinish+"'}}";

    bdiv.appendChild (bt0);
    if (ebutton) bdiv.appendChild (bte);
    bdiv.appendChild (bt1);
    bdiv.appendChild (bt2);
    elm.appendChild (bdiv);
    
    $(elm).insertAfter(e);
}

// --------------------------------------------------------------------------
// Implementation of the <u-radiogroup> tag. Represents a group of 
// radio-buttons bound to a single value. The options are represented
// by a comma-separated listin the 'options' attribute of the tag.
// Individual options either share an id and label, or consist of both
// separated by a colon.
// --------------------------------------------------------------------------
MarkupDecorators.radiogroup = function (e, mname) {
    var modelroot = Markup.getParentAttribute (e, "u-modelbase", ".");
    var submitmethod = Markup.getParentAttribute (e, "u-cb");

    var label = e.innerHTML;
    var optstr = e.getAttribute ("options");
    var options = optstr.split (',');
    var groupid = e.id;
    var nelm;

    nelm = document.createElement("br");
    $(nelm).insertAfter(e);

    var div = document.createElement("div");
    div.setAttribute ("class","uRadioGroup");

    if (options.length) {
        for (var i=options.length-1;i>=0;--i) {
            var spl = options[i].split(':');
            var optid = spl[0];
            var optname = optid;
            if (spl.length>1) optname = spl[1];

            var id = mname + "-" + groupid + "-" + optid;

            nelm = document.createElement ("input");
            nelm.id = id;
            nelm.setAttribute ("name", groupid);
            nelm.setAttribute ("value", optid);
            nelm.setAttribute ("v-model", modelroot + groupid);
            nelm.setAttribute ("u-model", modelroot + groupid);
            nelm.setAttribute ("type", "radio");
            nelm.setAttribute ("onclick", "this.blur();");
            if (submitmethod) {
                nelm.setAttribute ("v-on:change", submitmethod);
            }
            div.appendChild(nelm);

            nelm = document.createElement ("label");
            nelm.setAttribute ("for", id);
            nelm.setAttribute ("class", "radio");
            nelm.innerHTML = optname;
            div.appendChild(nelm);
        }

        $(div).insertAfter (e);
        nelm = document.createElement ("label");
        nelm.innerHTML = e.innerHTML;
        $(nelm).insertAfter (e);
    }
}

MarkupDecorators.header = function (e, mname) {
    var label = e.innerHTML;
    var icon = e.getAttribute ("icon");
    
    var span = document.createElement ("div");
    span.className = "iconheader"
    var nelm;

    nelm = document.createElement ("div");
    nelm.className = "headericon";
    nelm.style.backgroundImage = "url('icon/" + icon + "')";
    
    span.appendChild (nelm);
    
    nelm = document.createElement ("span");
    nelm.innerHTML = label;
    span.appendChild (nelm);
    
    $(span).insertAfter (e);
}

MarkupDecorators.meter = function (e, mname) {
    var usevar = e.getAttribute ("var");
    var width = e.getAttribute ("width");
    var xclass = e.getAttribute ("class");
    var addclass = xclass ? " "+xclass : "";
    if (! width) width = 120;
    
    nelm = document.createElement ("span");
    let html = '<table cellspacing="0" class="meter'+addclass+'" width="'+width+'"><tr>';
    html += '<td v-bind:width="parseInt(' + usevar + ')*0.01*'+width+
            '" class="meterfill">&nbsp;</td>';
    html += '<td v-bind:width="' + "(100-parseInt(" + usevar + ')) * 0.01*'+
             width+'" class="meterempty">'+'&nbsp;</td>';
    html += "</tr></table>"
    nelm.innerHTML = html;
    
    $(nelm).insertAfter (e);
}

// --------------------------------------------------------------------------
// Decorator for <u-button>.
// --------------------------------------------------------------------------
MarkupDecorators.button = function (e, mname) {
    var label = e.innerHTML;
    var icon = e.getAttribute ("icon");
    var cb = e.getAttribute ("cb");
    var xclass = e.getAttribute ("class");
    var uclass = e.getAttribute ("bindclass");
    var req = e.getAttribute ("require");
    var role = e.getAttribute ("role");
    var meta = e.getAttribute ("meta");
    var badge = e.getAttribute ("badge");
    var vbadge = e.getAttribute ("v-badge");
    var parentBadge = e.getAttribute ("parent-badge");
    var nelm;

    if (xclass && xclass.indexOf('tabgroup')>=0) {
        icon = undefined;
    }
    
    nelm = document.createElement ("button");
    if (e.id) {
        nelm.id = mname + "-" + e.id;
    }
    if (icon) {
        nelm.className = "withicon";
        nelm.style.backgroundImage = "url('icon/" + icon + "')";
    }
    if (req) {
        nelm.setAttribute ("v-bind:disabled","!("+req+")");
    }
    if (uclass) {
        nelm.setAttribute ("v-bind:class", uclass);
    }
    if (role) {
        nelm.setAttribute ("u-role", role);
    }
    if (meta) {
        nelm.setAttribute ("u-meta", meta);
    }
    nelm.setAttribute ("type", "button");
    if (cb) nelm.setAttribute ("v-on:click", cb);
    nelm.setAttribute ("onclick","$(this).blur()");
    
    var ediv = document.createElement ("div");
    ediv.style.position = "relative";
    
    var espan = document.createElement("span");
    espan.innerHTML = label;
    ediv.appendChild (espan);
    
    if (xclass) $(nelm).addClass (xclass);
    var kb = $(espan).find('u');
    if (kb.length) {
        nelm.setAttribute ("u-key", kb[0].innerText.toLowerCase());
    }
    
    if (badge || vbadge) {
        if (badge) nelm.setAttribute ("u-badge", badge);
        else nelm.setAttribute ("v-bind:u-badge", vbadge);
        if (parentBadge) nelm.setAttribute ("u-badge-parent", parentBadge);
        nelm.setAttribute ("u-badge-mounted",0);
        var bdiv = document.createElement("div");
        bdiv.setAttribute ("class","badge");
        bdiv.setAttribute("v-static","keep");
        bdiv.innerHTML = "0";
        ediv.appendChild (bdiv);
    }
    
    nelm.appendChild (ediv);
    $(nelm).insertAfter (e);
}

MarkupDecorators.button.mountBadges = function(jq) {
    let q = jq.find("[u-badge-mounted=0]");
    for (let nelm of q) {
        let parent = nelm.getAttribute("u-badge-parent");
        nelm.setAttribute("u-badge-mounted","1");
        let bdiv = $(nelm).find(".badge")[0];
        nelm.addEventListener ("increaseBadge", function(ev) {
            let bdiv = $(nelm).find(".badge")[0];
            let count = ev.count;
            if (! count) count = 1;
            bdiv.innerHTML = parseInt(bdiv.innerHTML)+count;
            bdiv.classList.add ("nonzero");
            if (parent) {
                App.increaseBadge (parent, count);
            }
        });
        
        nelm.addEventListener ("setBadge", function(ev) {
            let bdiv = $(nelm).find(".badge")[0];
            var oldcount = parseInt (bdiv.innerHTML);
            bdiv.innerHTML = ev.count;
            if (! ev.count) bdiv.classList.remove ("nonzero");
            else if (! oldcount) bdiv.classList.add ("nonzero");
            if (parent) {
                let diff = ev.count - oldcount;
                if (diff) {
                    App.increaseBadge (parent, diff);
                }
            }

        });
        
        nelm.addEventListener ("clearBadge", function() {
            let bdiv = $(nelm).find(".badge")[0];
            var oldcount = parseInt (bdiv.innerHTML);
            bdiv.innerHTML = "0";
            bdiv.classList.remove ("nonzero");
            if (oldcount && parent) {
                App.increaseBadge (parent, -oldcount);
            }
        });
    }
}

// --------------------------------------------------------------------------
// Decorator for the <u-listkeyboard> tag. This creeates a hidden
// input that will be targeted by the global keyboard handler
// for cursor up/down keyboard events going towards the
// uSelectList.
// --------------------------------------------------------------------------
MarkupDecorators.listkeyboard = function (e, mname) {
    nelm = document.createElement ("input");
    nelm.id = mname + "-keyboard";
    nelm.className = "keyboardcatch";
    nelm.setAttribute ("onkeydown",
        "MarkupDecorators.listkeyboard.keyHandler(event,"+mname+
        ",'"+mname+"')");
    $(nelm).insertAfter (e);
}

// --------------------------------------------------------------------------
// Keyboard handler for <u-listkeyboard>. Generally speaking this will
// only receive second hand events.
// --------------------------------------------------------------------------
MarkupDecorators.listkeyboard.keyHandler = function(e,obj,mname) {
    qall = $("#"+mname+" .uSelectList li:not(.group)");
    qsel = $("#"+mname+" .uSelectList li.selected");
    
    switch (e.keyCode) {
        case 27: // escape
            obj.deSelect();
            e.stopPropagation();
            break;

        case 38: // up
            if (e.ctrlKey || e.metaKey) {
                App.sideBarCursor (event);
                e.stopPropagation();
                break;
            }
            if (qsel.length) {
                let qc = qall.length;
                for (let i=1; i<qc; ++i) {
                    if (qall[i].classList.contains("selected")) {
                        qall[i].classList.remove("selected");
                        Markup.lazyScroll(qall[i-1],qall[i-1].parentNode);
                        if (obj.clickToSelect) obj.clickToSelect();
                        qall[i-1].click();
                        break;
                    }
                }
            }
            e.stopPropagation();
            break;
        
        case 40: // down
            if (e.ctrlKey || e.metaKey) {
                App.sideBarCursor (event);
                e.stopPropagation();
                break;
            }
            if (qsel.length) {
                let qc = qall.length;
                for (let i=0; i<qc-1; ++i) {
                    if (qall[i].classList.contains("selected")) {
                        qall[i].classList.remove("selected")
                        Markup.lazyScroll(qall[i+1],qall[i+1].parentNode);
                        if (obj.clickToSelect) obj.clickToSelect();
                        qall[i+1].click();
                        break;
                    }
                }
            }
            else {
                Markup.lazyScroll(qall[0], qall[0].parentNode);
                if (obj.clickToSelect) obj.clickToSelect();
                $(qall[0]).click();
            }
            e.stopPropagation();
            break;

        case 13: // enter
            qsel[0].click();
            e.stopPropagation();
            break;

        case 37: // left
            if (event.ctrlKey || event.metaKey) App.tabGroupCursor(event);
            else return true;
            break;

        case 39: // right
            if (event.ctrlKey || event.metaKey) App.tabGroupCursor(event);
            else return true;
            break;
    }
    e.preventDefault();
    return false;
}

// --------------------------------------------------------------------------
// Decorator for <u-searchinput>. Handles the keyboard scrolling for
// search results as well.
// --------------------------------------------------------------------------
MarkupDecorators.searchinput = function (e, mname) {
    var cb = e.getAttribute ("cb");
    var name = e.getAttribute ("name");
    var type = e.getAttribute ("type");
    var ph = e.innerHTML;
    var nelm;

    if (!type) type = "navigate";
    
    nelm = document.createElement ("input");
    nelm.setAttribute ("v-model", name);
    nelm.setAttribute ("v-on:input", cb);
    nelm.setAttribute ("type", "text");
    nelm.setAttribute ("placeholder", ph);
    nelm.setAttribute ("spellcheck", "false");
    nelm.setAttribute ("onkeydown",
        "MarkupDecorators.searchinput.keyHandler(event,"+mname+
        ",'"+mname+"','"+type+"')");  
    $(nelm).insertAfter (e);
}

// --------------------------------------------------------------------------
// Keyboard handler for the <u-searchinput> tag.
// --------------------------------------------------------------------------
MarkupDecorators.searchinput.keyHandler = function(e,obj,mname,type) {
    qall = $("#"+mname+" .uSearchResults li");
    qsel = $("#"+mname+" .uSearchResults li.selected");
    
    switch (e.keyCode) {
        case 38: // up
            if (e.ctrlKey || e.metaKey) {
                App.sideBarCursor (e);
                break;
            }
            if (qsel.length) {
                let qc = qall.length;
                for (let i=1; i<qc; ++i) {
                    if (qall[i].classList.contains("selected")) {
                        qall[i].classList.remove("selected");
                        Markup.lazyScroll(qall[i-1],qall[i-1].parentNode);
                        if (type=="select") {
                            qall[i-1].click();
                        }
                        else {
                            qall[i-1].classList.add("selected")
                            qall[i].classList.remove("selected");
                        }
                    }
                }
            }
            break;
        
        case 40: // down
            if (e.ctrlKey || e.metaKey) {
                App.sideBarCursor (e);
                break;
            }
            if (qsel.length) {
                let qc = qall.length;
                for (let i=0; i<qc-1; ++i) {
                    if (qall[i].classList.contains("selected")) {
                        Markup.lazyScroll(qall[i+1],qall[i+1].parentNode);
                        if (type =="select") {
                            qall[i+1].click();
                        }
                        else {
                            qall[i+1].classList.add ("selected");
                            qall[i].classList.remove ("selected");
                        }
                        break;
                    }
                }
            }
            else {
                if (qall.length) {
                    Markup.lazyScroll (qall[0], qall[0].parentNode);
                    if (type=="select") qall[0].click();
                    else qall[0].classList.add ("selected");
                }
            }
            break;
            
        case 13: // enter
            if (type=="select") return;
            qsel.click();
            qsel[0].classList.remove ("selected");
            break;
            
        case 27: // escape
            obj.clearSearch();
            e.stopPropagation();
            break;

        case 37: // left
            if (e.ctrlKey || e.metaKey) App.tabGroupCursor (e);
            else return true;
            break;

        case 39: // right
            if (e.ctrlKey || e.metaKey) App.tabGroupCursor (e);
            else return true;
            break;
            
        default: return;
    }

    e.preventDefault();
    return false;
}

// --------------------------------------------------------------------------
// Decorator for the <u-textinput> tag. Acts like a bound form input,
// but pressing enter will blur the field.
// --------------------------------------------------------------------------
MarkupDecorators.textinput = function (e, mname) {
    var modelroot = Markup.getParentAttribute (e, "u-modelbase",".");
    var submitmethod = Markup.getParentAttribute (e, "u-cb");
    var validate = Markup.getParentAttribute (e, "u-validate");
    var vcheckbox = e.getAttribute ("u-editcheckbox");
    var vdefaulttext = e.getAttribute ("u-default");

    var nelm;
    var parent = e.parentNode;
    var tp = e.getAttribute("type");
    var sz = e.getAttribute("size");
    var label = e.innerHTML;
    var placeholder = "";
    var ac = e.getAttribute("ac");
    var brk = e.getAttribute("break");
    var vname = e.getAttribute("v-name");
    var eid = e.id;
    var vindex = e.getAttribute("vindex");
    var nummin = e.getAttribute("min");
    var nummax = e.getAttribute("max");

    // Usage: <u-textinput>First name ((John))</u-textinput>
    // Turns 'John' into the placeholder text.
    if (label.indexOf('((')>0) {
        var idx = label.indexOf('((');
        placeholder = label.substring (idx+2,label.length-2);
        label = label.substring (0,idx);
    }
    
    // Linebreak explicitly after the input, unless if requested otherwise.
    if (brk != "no") {
        nelm = document.createElement ("br");
        $(nelm).insertAfter (e);
    }

    nelm = document.createElement ("input");

    if (vcheckbox) {
        nelm.setAttribute ("v-show",vcheckbox);
        nelm.setAttribute ("u-debug-show",vcheckbox);
    }

    // Figure out how to set the 'name' attribute, either bound
    // to a value in the Vue view through v-name, or just by copying
    // the <u-textinput> object's id attribute.
    if (vname) {
        nelm.setAttribute ("v-bind:name",vname);
    }
    else {
        nelm.setAttribute ("name", e.id);

        // We'll use vname later on for validation. So construct one
        // from the name we actually set.
        vname = "'"+modelroot+e.id+"'";
    }
    nelm.setAttribute ("id", mname + '-' + e.id);
    if (tp) nelm.setAttribute ("type",tp);
    if (placeholder) nelm.setAttribute ("placeholder",placeholder);
    
    nelm.setAttribute ("v-model", modelroot + e.id);
    if (submitmethod) nelm.setAttribute ("v-on:input", submitmethod);
    nelm.setAttribute ("v-on:keyup",
                       "MarkupDecorators.textinput.blur(event)");  
    nelm.setAttribute ("onfocus",
                       "MarkupDecorators.textinput.onfocus (event)");
    if (sz) {
        if (tp=="number") {
            let pxsz = 16 + (parseInt(sz) * 12);
            nelm.setAttribute ("style","width: "+pxsz+"px;");
        }
        else { 
            nelm.setAttribute ("size", e.getAttribute("size"));
        }
    }
    else {
        nelm.setAttribute ("style","width: 300px;");
        nelm.setAttribute ("size", "40");
    }
    if (tp == "number") {
        if (nummin) nelm.setAttribute("min",nummin);
        if (nummax) nelm.setAttribute("max",nummax);
    }
    $(nelm).insertAfter (e);
    
    if (vcheckbox && vdefaulttext) {
        nelm = document.createElement ("span");
        nelm.setAttribute ("class","textvalue");
        nelm.setAttribute ("v-if","!" + vcheckbox);
        nelm.innerHTML = "{{" + vdefaulttext + "}}";
    }
    
    $(nelm).insertAfter (e);
    
    nelm = document.createElement ("label");
    nelm.setAttribute ("for", mname + '-' + e.id);

    // If there's a validation callback, tie it to
    // the label's 'error' css class.
    if (validate) {
        if (!vindex) vindex="null";
        nelm.setAttribute ("v-bind:class",
                           "{'error':!"+validate+"("+
                           vname+","+
                           modelroot + e.id+","+vindex+")}");
        nelm.setAttribute ("u-bind:class",
                           "{'error':!"+validate+"("+
                           vname+","+
                           modelroot + e.id+","+vindex+")}");
    }
    if (vcheckbox) {
        let checkhtml = '<input type="checkbox" v-model="'+vcheckbox+'">' +
                        '&nbsp; ';
        label = checkhtml + label;
    }
    nelm.innerHTML = label;
    $(nelm).insertAfter (e);
}

// --------------------------------------------------------------------------
// Onfocus handler, keeps track of the value as it was before the user
// started messing with it, so that it can be restored when he/she presses
// the escape key.
// --------------------------------------------------------------------------
MarkupDecorators.textinput.onfocus = function(e) {
    e.target.setAttribute ("u-origvalue", $(e.target).val());
}

// --------------------------------------------------------------------------
// Keyboard handler for the <u-textinput> tag.
// --------------------------------------------------------------------------
MarkupDecorators.textinput.blur = function(e) {
    if (e.keyCode == 13) $(e.target).blur();
    else if (e.keyCode == 27) {
        if ($(e.target).val() != "") {
            var repl = e.target.getAttribute('u-origvalue');
            if (!repl) repl = "";
            const event = new Event('input', {
                cancelable: true,
                bubbles: true
            });
            $(e.target).val(repl);
            e.target.dispatchEvent (event);
            $(e.target).blur();
        }
        else {
            var q = $(".backbutton:visible");
            if (q.length>1) {
                q = $(".uDialog .backbutton:visible")
                if (q.length == 0) {
                    q = $(".uWizard .backbutton:visible")
                }
            }
            q.click();
            event.preventDefault();
            return false;
        }
    }
    else $(e.target).focus();
}

// --------------------------------------------------------------------------
// Decorator for the <u-currency> tag.
// --------------------------------------------------------------------------
MarkupDecorators.currency = function (e, mname) {
    var modelroot = Markup.getParentAttribute (e, "u-modelbase",".");
    let nelm = document.createElement ("input");
    nelm.setAttribute ("size", 5);
    nelm.setAttribute ("v-model", modelroot + e.id);
    nelm.setAttribute ("style","text-align: right; margin-right: -3px;");
    var submitmethod = Markup.getParentAttribute (e, "u-cb");
    if (submitmethod) nelm.setAttribute ("v-on:input", submitmethod);
    nelm.setAttribute ("onkeydown","MarkupDecorators.currency.key(event)");
    nelm.setAttribute ("onblur","MarkupDecorators.currency.blur(event)");
    $(nelm).insertAfter (e);
}

MarkupDecorators.currency.key = function(e) {
    if (e.key == ".") {
        let str = String($(e.target).val());
        if (str.indexOf ('.') < 0) return;
        e.preventDefault();
    }
    if (e.keyCode < 58) return;
    if (e.keyCode > 95 && e.keyCode < 106) return;
    e.preventDefault();
}

MarkupDecorators.currency.blur = function(e) {
    let val = Number($(e.target).val());
    $(e.target).val (val.toFixed(2));
}

// --------------------------------------------------------------------------
// Decorator for the <u-select> tag.
// --------------------------------------------------------------------------
MarkupDecorators.select = function (e, mname) {
    var modelroot = Markup.getParentAttribute (e, "u-modelbase",".");
    var submitmethod = Markup.getParentAttribute (e, "u-cb");
    if (! submitmethod) submitmethod = e.getAttribute ("u-cb");

    var brk = e.getAttribute("break");
    var nelm;

    if (brk != "no") {
        nelm = document.createElement ("br");
        $(nelm).insertAfter (e);
    }

    nelm = document.createElement ("select");
    nelm.setAttribute ("name", e.id);
    nelm.setAttribute ("id", mname + "-" + e.id);
    nelm.setAttribute ("v-model", modelroot + e.id);
    nelm.setAttribute ("onchange","this.blur()");
    if (submitmethod) nelm.setAttribute ("v-on:change", submitmethod);
    nelm.innerHTML = e.innerHTML;
    $(nelm).insertAfter(e);

    nelm = document.createElement ("label");
    nelm.setAttribute ("for", mname + '-' + e.id);
    nelm.innerHTML = e.getAttribute ("label");
    $(nelm).insertAfter (e);
}

MarkupDecorators.textarea = function (e, mname) {
    var modelroot = Markup.getParentAttribute (e, "u-modelbase",".");
    var submitmethod = Markup.getParentAttribute (e, "u-cb");
    if (! submitmethod) submitmethod = e.getAttribute ("u-cb");
    var rows = e.getAttribute ("rows");
    var cols = e.getAttribute ("cols");
    if (! rows) rows = 5;
    if (! cols) cols = 40;

    var brk = e.getAttribute("break");
    var nelm;

    if (brk != "no") {
        nelm = document.createElement ("br");
        $(nelm).insertAfter (e);
    }

    nelm = document.createElement ("textarea");
    nelm.setAttribute ("name", e.id);
    nelm.setAttribute ("id", mname + "-" + e.id);
    nelm.setAttribute ("rows", rows);
    nelm.setAttribute ("cols", cols);
    nelm.setAttribute ("v-model", modelroot + e.id);
    if (submitmethod) nelm.setAttribute ("v-on:change", submitmethod);
    nelm.innerHTML = e.innerHTML;
    $(nelm).insertAfter(e);

    nelm = document.createElement ("label");
    nelm.setAttribute ("class", "textarea");
    nelm.setAttribute ("for", mname + '-' + e.id);
    nelm.innerHTML = e.getAttribute ("label");
    $(nelm).insertAfter (e);
}

// --------------------------------------------------------------------------
// Decorator for the <u-countryselect> tag. Populates a <select> with
// ISO country codes.
// --------------------------------------------------------------------------
MarkupDecorators.countryselect = function (e, mname) {
    var modelroot = Markup.getParentAttribute (e, "u-modelbase",".");
    var submitmethod = Markup.getParentAttribute (e, "u-cb");
    
    var nelm = document.createElement ("br");
    $(nelm).insertAfter (e);

    nelm = document.createElement ("select");
    nelm.setAttribute ("name", e.id);
    nelm.setAttribute ("id", mname + "-" + e.id);
    nelm.setAttribute ("v-model", modelroot + e.id);
    nelm.setAttribute ("onchange","this.blur()");
    nelm.setAttribute ("v-on:change", submitmethod);
    
    for (cc in ISOCC) {
        var optelm = document.createElement ("option");
        optelm.setAttribute ("value", cc);
        optelm.innerHTML = ISOCC[cc];
        $(nelm).append (optelm);
    }
    
    $(nelm).insertAfter (e);

    nelm = document.createElement ("label");
    nelm.setAttribute ("for", mname + '-' + e.id);
    nelm.innerHTML = e.innerHTML;
    $(nelm).insertAfter (e);
}
