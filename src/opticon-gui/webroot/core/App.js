// ==========================================================================
// Collection of global app-related functions
// ==========================================================================

App = {
    lastDeepLink:{},
    logoutListeners:[],
    loginListeners:[]
};

// --------------------------------------------------------------------------
// Start up 
// --------------------------------------------------------------------------
App.activate = function(defaultpath) {
    var self = App;
    var inhash = window.location.hash.split('#')[1];

    if (inhash && inhash.indexOf ("&auth_token=") >= 0) {
        let token = inhash.replace (/.*&auth_token=/, "");
        API.Auth.setToken (token, function (res) {
            inhash = inhash.split('&')[0];

            if ((!inhash) || (inhash=="") || (inhash=="/")) {
                inhash = defaultpath;
            }

            Router.activate (inhash);
            self.handle (inhash);
        });
        return;
    }

    if ((!inhash) || (inhash=="") || (inhash=="/")) {
        inhash = defaultpath;
    }
    
    Router.activate (inhash);
    self.handle (inhash);
}

// --------------------------------------------------------------------------
// Display the loading spinner.
// --------------------------------------------------------------------------
App.busy = function() {
    $("#busy").addClass ("busy");
}

// --------------------------------------------------------------------------
// Hide the loading spinner.
// --------------------------------------------------------------------------
App.done = function() {
    $("#busy").removeClass ("busy");
}

// --------------------------------------------------------------------------
// Hide deep links on logout.
// --------------------------------------------------------------------------
App.clearDeepLinks = function() {
    App.lastDeepLink = {};
}

// --------------------------------------------------------------------------
// In case we're ending up in different sidebar chapters, keep track of
// the last page we visited before we move on, so that navigating back
// to that chapter brings the user back to that page, rather than the
// chapter's root.
// --------------------------------------------------------------------------
App.rememberLink = function(path) {
    var splt = path.split('/');
    var idx = splt[1];
    App.lastDeepLink[idx] = path;
}

// --------------------------------------------------------------------------
// Jump to a specific path.
// --------------------------------------------------------------------------
App.go = function(path) {
    Router.go (path);
    App.handle (path);
}

// --------------------------------------------------------------------------
// Jump to either the root, or the last visited deep link of a sub-root.
// --------------------------------------------------------------------------
App.goDeep = function(path) {
    var splt = path.split('/');
    var idx = splt[1];
    if (App.lastDeepLink[idx]) App.go (App.lastDeepLink[idx]);
    else App.go (path);
}

// --------------------------------------------------------------------------
// Handle incoming path. Sets the sidebar status.
// --------------------------------------------------------------------------
App.handle = function (path) {
    App.rememberLink (path);
    $(".uMenuItem.uSelected").removeClass("uSelected");
    var rootpath = path.split('/')[1];
    if (rootpath) {
        q=".uMenuItem[u-route='/"+rootpath+"']";
        $(q).addClass ("uSelected");
    }
}

// --------------------------------------------------------------------------
// Retrieve the system tag, and display it if set.
// --------------------------------------------------------------------------
App.serviceTag = function() {
    /*
    API.getServiceTag (function (tag) {
        if (tag) {
            $("#servicetag").html(tag + "<br><br>");
        }
    }); */
}

// --------------------------------------------------------------------------
// Handler for ctrl+cursor up/down, called from different keyboard handlers.
// --------------------------------------------------------------------------
App.sideBarCursor = function (event) {
    if (event.keyCode == 38) {
        $(".uMenuItem.uSelected:not(:first-child)").prevAll(":visible:first").click();
    }
    else {
        var q = $(".uMenuItem.uSelected").nextAll(":visible:first");
        if (q[0].hasAttribute("u-route")) q.click();
    }
}

// --------------------------------------------------------------------------
// Handler for ctrl+cursor left/right.
// --------------------------------------------------------------------------
App.tabGroupCursor = function(event) {
    var qstr = "button.tabgroup.middle.active:visible";
    if (event.keyCode==37) {
        qstr += ",button.tabgroup.right.active:visible";
    }
    else {
        qstr += ",button.tabgroup.left.active:visible";
    }
    q=$(qstr);
    if (q.length > 0) {
        if (event.keyCode==37) {
            q.prev().click();
        }
        else {
            q.next().click();
        }
    }
    else {
        var qstr = "button.ui-tab:not(:first-child):not(:last-child)"+
                   ".active:visible";
        if (event.keyCode==37) {
            qstr += ",button.ui-tab:last-child.active:visible";
        }
        else {
            qstr += ",button.ui-tab:first-child.active:visible";
        }
        q=$(qstr);
        if (q.length > 0) {
            if (event.keyCode==37) {
                q.prev().click();
            }
            else {
                q.next().click();
            }
        }
    }
}

// --------------------------------------------------------------------------
// Show altkey shortcut hints where applicable
// --------------------------------------------------------------------------
App.showAltHints = function() {
    $("[u-key] u").addClass('altpressed');
}

// --------------------------------------------------------------------------
// Hide abovementioned hints.
// --------------------------------------------------------------------------
App.hideAltHints = function() {
    $("[u-key] u.altpressed").removeClass('altpressed');
}   

// --------------------------------------------------------------------------
// Handler for alt+key presses.
// @param event The event
// @param down True if keydown
// --------------------------------------------------------------------------
App.altKeyHandler = function(event,down) {
    if (event.keyCode == 18) {
        return down ? App.showAltHints() : App.hideAltHints();
    }

    if (! down) return;

    var key = String.fromCharCode(event.keyCode).toLowerCase();
    var query = "[u-key='"+key+"']:visible";
    if (event.keyCode>48 && event.keyCode <58) {
        var qq=$(".uDialog:visible,.uWizard:visible");
        if (qq.length == 0) {
            var index = event.keyCode-49;
            qq=$(".tabgroup:visible");
            if (index<qq.length) qq[index].click();
            event.preventDefault();
            return false;
        }
    }

    var q = $(query);
    if (q.length) {
        q.click();
        event.preventDefault();
        return false;
    }
}

// --------------------------------------------------------------------------
// Handler for tab key presses. If no input element is currently focused,
// this will focus the first relevant one.
// --------------------------------------------------------------------------
App.tabKeyHandler = function(event) {
    var etag = document.activeElement.tagName;
    if (["INPUT","SELECT","TEXTAREA"].indexOf (etag) < 0) {
        // Tab key. Do something nice.
        var e=$(".uDialog input:visible,"+
                ".uDialog select:visible,"+
                ".uWizard input:visible,"+
                ".uWizard select:visible,"+
                ".uSearch input:visible,"+
                ".uEditable:visible input").first();
        $(e).focus();
        event.preventDefault();
    }
}

// --------------------------------------------------------------------------
// Handler for the enter key. Clicks the relevant button if possible.
// --------------------------------------------------------------------------
App.enterKeyHandler = function(event) {
    if ($(":focus").is(".trumbowyg-editor")) return;
    var q = $(".enteraction:visible");
    if (q.length) {
        $(document.activeElement).blur();
        q.click();
        event.preventDefault();
        return false;
    }
    else {
        q = $(Module.Page.current.Vidi.$el);
        q = q.find(".keyboardcatch");
        if (q.length) {
            $(q[0]).trigger (event);
            event.stopPropagation();
            return false;
        }
        else {
            if (Module.Page.current.enterKey) {
                if (Module.Page.current.enterKey (event)) {
                    event.preventDefault();
                    return false;
                }
            }
            else {
                event.preventDefault();
                return false;
            }
        }
    }
}

// --------------------------------------------------------------------------
// Handler for the escape key, specifically in cases where no more useful
// element has control over the keyboard. Will either click on a visible
// 'back' button, or forward it to the keyboardcatch.
// --------------------------------------------------------------------------
App.escapeKeyHandler = function(eent) {
    if ($(":focus").is(".trumbowyg-editor")) {
        let q = $(":focus").parent().parent().next();
        q.find("button[u-meta='esc']").click();
        return;
    }
    if (document.activeElement.className == "uDoc") {
        var q = $(".backbutton:visible");
        if (q.length>0) {
            if (q.length>1) {
                q = $(".uDialog .backbutton:visible")
                if (q.length == 0) {
                    q = $(".uWizard .backbutton:visible")
                }
            }
            q.click();
            event.preventDefault();
        }
        else {
            // ok, maybe there's a selectlist active
            // and we can fire this to let it clear
            // its selection.
            q = $(".uDialog:visible,.uWizard:visible");
            if (q.length == 0) {
                q = $(Module.Page.current.Vidi.$el)
                q = q.find(".keyboardcatch");
                if (q.length) {
                    $(q[0]).trigger (event);
                    event.stopPropagation();
                }
            }
        }
        return false;
    }
}

// --------------------------------------------------------------------------
// Handler for the cursor up/down keys. Will forward to the sidebar cursor
// if pressed with ctrl, or any keyboard catch currently active.
// --------------------------------------------------------------------------
App.cursorUpDownHandler = function(event) {
    if ($(":focus").is(".trumbowyg-editor")) return;
    if (document.activeElement.className == "uDoc") {
        if (event.ctrlKey || event.metaKey) {
            App.sideBarCursor (event);
            event.stopPropagation();
            return false;
        }
        var q = $(".uDialog:visible,.uWizard:visible");
        if (q.length) return;
        q = $(Module.Page.current.Vidi.$el).find(".keyboardcatch");
        if (q.length) {
            $(q[0]).trigger (event);
            event.stopPropagation();
            return false;
        }
    }
    else if (document.activeElement.type == "textarea") {
        if (event.ctrlKey || event.metaKey) {
            App.sideBarCursor (event);
            event.stopPropagation();
            return false;
        }
    }
}

// --------------------------------------------------------------------------
// Global handler for cursor left/right. If pressed with ctrl, navigate
// whatever tabgroup is currently visible.
// --------------------------------------------------------------------------
App.cursorLeftRightHandler = function(event) {
    if ($(":focus").is(".trumbowyg-editor")) return;
    var q = $(".uDialog:visible,.uWizard:visible");
    if (q.length) return;

    if (! (event.ctrlKey||event.metaKey)) return;

    App.tabGroupCursor (event);
    event.stopPropagation();
    return false;
}

// --------------------------------------------------------------------------
// Increases the count for a badge.
// --------------------------------------------------------------------------
App.increaseBadge = function (id,howmuch) {
    console.log ("[Badge] increase "+id+" by "+howmuch);
    let q = $("[u-badge='"+id+"']");
    if (q.length == 0) return;
    let e = new Event ("increaseBadge");
    if (howmuch) e.count = howmuch;
    q[0].dispatchEvent (e);
}

// --------------------------------------------------------------------------
// Set the count for a badge.
// --------------------------------------------------------------------------
App.setBadge = function (id,howmuch) {
    console.log ("[Badge] set "+id+" to "+howmuch);
    let q = $("[u-badge='"+id+"']");
    if (q.length == 0) return;
    let e = new Event ("setBadge");
    e.count = howmuch;
    q[0].dispatchEvent (e);
}

// --------------------------------------------------------------------------
// Clear the count for a badge.
// --------------------------------------------------------------------------
App.clearBadge = function (id) {
    console.log ("[Badge] clear "+id);
    let q = $("[u-badge='"+id+"']");
    if (q.length == 0) return;
    q[0].dispatchEvent (new Event ("clearBadge"));
}

// --------------------------------------------------------------------------
// Add a callback hook to the logout procedure
// --------------------------------------------------------------------------
App.addLogoutHandler = function(f) {
    App.logoutListeners.push(f);
}

// --------------------------------------------------------------------------
// Kill everything related to the current session.
// --------------------------------------------------------------------------
App.shutDown = function() {
    for (let listener of App.logoutListeners) {
        listener();
    }
}

App.menuRoles = function() {
    let q = $(".uMenuItem");
    let roles = API.Auth.credentials.roles;
    for (let i=0; i<q.length; ++i) {
        let show = true;
        if (q[i].hasAttribute("u-role")) {
            show = false;
            let role = q[i].getAttribute("u-role");
            if (roles.indexOf("ADMIN") >= 0) show = true;
            else if (roles.indexOf(role) >= 0) show = true;
        }
        if (show && q[i].hasAttribute("u-module")) {
            let module = q[i].getAttribute("u-module");
            API.mkURL (module, "/", function (val) {
                if (val === null) $(q[i]).hide();
                else $(q[i]).show();
            });
            continue;
        }
        if (show) $(q[i]).show();
        else $(q[i]).hide();
   }
}

// --------------------------------------------------------------------------
// Add a callback hook for successful login
// --------------------------------------------------------------------------
App.addLoginHandler = function(f) {
    App.loginListeners.push(f);
}

// --------------------------------------------------------------------------
// Execute all login callback hooks.
// --------------------------------------------------------------------------
App.handleLogin = function() {
    App.menuRoles();
    App.serviceTag();
    for (let listener of App.loginListeners) {
        listener();
    }
}
