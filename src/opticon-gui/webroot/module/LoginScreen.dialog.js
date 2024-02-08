// ==========================================================================
// Login dialog.
// ==========================================================================
LoginScreen = new Module.Dialog ("LoginScreen");

// --------------------------------------------------------------------------
// Constructor
// --------------------------------------------------------------------------
LoginScreen.create = function() {
    var self = LoginScreen;
    self.createView({
        username:"",
        password:"",
        error:"",
        autofilled:false,
        requireAuthCode: false
    });
    self.isOpen = false;
}

// --------------------------------------------------------------------------
// Opens the dialog.
// --------------------------------------------------------------------------
LoginScreen.open = function() {
    var self = LoginScreen;
    self.View.username = "";
    self.View.password = "";
    self.View.authcode = "";
    self.View.error = "";
    
    console.log ("[Login] hash: " + window.location.hash)
    
    if (window.location.hash.indexOf('&auth_token=')>=0) {
        self.hide();
        App.handleLogin();
        App.activate ("/Server");
        return;
    }
    
    self.show();
    self.isOpen = true;
    $(".uSideBar").hide();
    $(".uContent").hide();
    $("#LoginScreen-username").focus();
    
    if (conf.autologin && conf.autologin.username) {
        self.View.username = conf.autologin.username;
        self.View.password = conf.autologin.password;
    }
    
    // Chrome autofill isn't immediately available, so wait for a bit.
    setTimeout (function(){
            // Autofill does not make the password available until
            // the user interacts with the page. The username
            // will be set, though, so trigger the vue validation
            // to ignore the 'empty' password, which will magically
            // be set once the user clicks or presses enter.
            var v = "" + $("#LoginScreen-username")[0].value;
            if (v.length) {
                self.View.autofilled=true;
            }
        }, 300);
}

LoginScreen.mockLogin = function (u, p, cb) {
    if (u == "admin" && p == "admin") cb(true);
    else cb (false);
}

// --------------------------------------------------------------------------
// Callback, try to authenticate the user.
// --------------------------------------------------------------------------
LoginScreen.submit = function() {
    var self = LoginScreen;
    $("#LoginScreen-submit").prop("disabled", true);
    
    API.Auth.login (self.View.username, self.View.password, self.View.authcode, function(r) {
        if (false === r || 403 === r) {
            self.View.password = "";
            self.View.authcode = "";
            self.View.error = "Authentication failed";
            $("#LoginScreen-error").show();
            $(self.Vidi.$el).shake(function() {
                $("#LoginScreen-error").fadeOut (2000);
                $("#LoginScreen-password").focus();
            });
        }
        else {
            // r can be true or a status code (e.g. 444)
            if (444 === r) {
                self.View.requireAuthCode = true
                $("#LoginScreen-authcode").focus() // focus on auth code field
                return
            }
            else {
                self.handleSuccessfulLogin()
            }
        }
    });
}

// --------------------------------------------------------------------------
// Handle successful login
// --------------------------------------------------------------------------
LoginScreen.handleSuccessfulLogin = function() {
    const self = LoginScreen;
    $("#LoginScreen-submit").prop ("disabled", false);
    $(".uSideBar").fadeIn();
    self.hide();
    self.isOpen = false;
    App.handleLogin();
    App.activate ("/Server");
}
