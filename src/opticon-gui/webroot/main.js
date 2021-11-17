// ==========================================================================
// Application startup logic and misc glue.
// ==========================================================================

// --------------------------------------------------------------------------
// Utility method for cloning an object, unsticking it from any references.
// Javascript is hell.
// --------------------------------------------------------------------------
function clone(obj) {
    if (null == obj || "object" != typeof obj) return obj;
    return JSON.parse (JSON.stringify(obj));
}

// --------------------------------------------------------------------------
// Logs the user out and resets to login screen.
// --------------------------------------------------------------------------
Logout = function() {
    App.go ("/");
    App.clearDeepLinks();
    App.shutDown();
    API.Identity.logout (function() {
        $(".uContent,.uDialog,.uWizard,.uSidebar").hide();
        LoginScreen.open();
        EventListener.stop();
    });
}

// --------------------------------------------------------------------------
// Startup function. Boots modules, and attaches keyboard handlers for
// the enter-key in inputs, and the escape key if nothing is focused.
// --------------------------------------------------------------------------
window.onload = function() {
    console.realerror = console.error;
    console.error = function(msg) {
        if (msg && msg.startsWith && msg.startsWith('[Vue')) {
            msg = msg.split('\n')[0];
            if (console.vueContext) {
                msg += " (" + console.vueContext +")";
            }
            console.warn (msg);
        }
        else console.realerror.apply (null, arguments);
    }
    Module.init (function() {
        $("form").attr("action","javascript:void(0);");
    
        LoginScreen.open();

        // Set up the delayed-expansion of collapsed buttons when hovered.
        var delay = 800;
        var tfunc;
        $("button").hover (function(ev){
            var that = this;
            tfunc = setTimeout (function() {
                $(that).addClass ("hover");
            }, delay)
        }, function() {
            clearTimeout (tfunc);
            $(this).removeClass ("hover");
        });

        // Make sure altkey-hints don't stay visible when a user alt-tabs out.
        $(window).blur (function (event){
            App.hideAltHints();
        });

        // Use the App's handler for the enter key in non-textarea inputs.
        $(document).on("keydown", ":input:not(textarea)", function(event) {
            if (event.keyCode == 13) return App.enterKeyHandler (event);
        });
        
        // Keyup handler specifically to turn off altkey hints.
        $(document).keyup (function (event){
            if (event.keyCode == 18) {
                return App.altKeyHandler (event, false);
            }
        });

        // Keydown handler. Delegate to the appropriate App handler.
        $(document).keydown (function (event) {
            // Shortcut keys with alt.
            if (event.keyCode == 18 && !event.metaKey) {
                return App.altKeyHandler (event, true);
            }

            switch (event.keyCode) {
                case 9: return App.tabKeyHandler (event);
                case 13: return App.enterKeyHandler (event);
                case 27: return App.escapeKeyHandler (event);
                case 37: return App.cursorLeftRightHandler (event);
                case 38: return App.cursorUpDownHandler (event);
                case 39: return App.cursorLeftRightHandler (event);
                case 40: return App.cursorUpDownHandler (event);
                default: break;
            }
        });
    });
}

URLRE=/http[s]{0,1}(([^:\/?#]+):)?(\/\/([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?/
EmojiRE=/^(\u00a9|\u00ae|[\u2000-\u3300]|\ud83c[\ud000-\udfff]|\ud83d[\ud000-\udfff]|\ud83e[\ud000-\udfff])+$/
