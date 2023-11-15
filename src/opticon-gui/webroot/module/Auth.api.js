API.Auth = new Module.API("Auth");

API.Auth.credentials = {
    token:"",
    access:"guest",
    username:"",
    password:"",
    tenant:"",
    roles:["GUEST"]
}

API.Auth.create = function() {
}

API.Auth.setToken = function (token, cb) {
    var self = API.Auth;
    self.credentials.token = token;
    if (conf.auth_method == "internal") {
        API.headers["X-Opticon-Token"] = token;
        API.get ("opticon","/token",function (err, data) {
            self.credentials.access = "user";
            if (data.token.userlevel == "AUTH_ADMIN") {
                self.credentials.access = "admin";
            };
            self.credentials.username = username;
            self.credentials.roles = ["USER"];
            self.credentials.tenant = data.token.tenants[0];
            cb (true);
        })
        return;
    }

    API.headers["X-Auth-Token"] = token;
    
    API.get ("account","/token",function (err, data) {
        self.credentials.access = "user";
        self.credentials.username = data.username;
        self.credentials.roles = data.roles;
        self.credentials.uuid = data.uuid;
        self.credentials.tenant = data.tenant;
        
        if (data.roles.indexOf("ADMIN")>=0) {
            self.credentials.access = "admin";
        }
        
        cb (true);
        if (! self.timeout) {
            $(window).focus(function (event) {
                self.checkAccess();
            });
            self.timeout = setTimeout (self.checkAccessJob, 30000);
        }
    });
}

API.Auth.login = function (username, password, cb) {
    var self = API.Auth;
    var svc;
    var path;
    
    if (conf.auth_method === undefined) {
        if (conf.url.unithost) {
            conf.auth_method = "unithost";
        }
        else conf.auth_method = "internal";
    }
    
    switch (conf.auth_method) {
        case "unithost":
            svc = "identity";
            path = "/"
            break;
        
        case "internal":
            svc = "opticon";
            path = "/login";
            break;
    }
    
    API.post (svc,path,{username,password}, function (err, data) {
        if (err) {
            cb (false);
            return;
        }
        
        if (data.token) {
            console.log ("[API] Login successful");
            self.setToken (data.token, cb);
            self.credentials.password = password;
        }
        else {
            console.log ("[API] Login failed");
            cb (false);
        }
    });
}

API.Auth.checkAccessJob = function() {
    var self = API.Auth;
    self.checkAccess();
    self.timeout = setTimeout (self.checkAccessJob, 30000);
}

API.Auth.checkAccess = function() {
    var self = API.Auth;
    if (self.credentials.token == "") return;
    
    API.get("account","/token",function(err,data) {
        if (err || (! data.authenticated)) {
            if (! self.credentials.username) {
                clearTimeout (self.timeout);
                Logout();
                return;
            }
            self.login (self.credentials.username,
                        self.credentials.password, function(res) {
                if (! res) {
                    clearTimeout (self.timeout);
                    Logout();
                }
            });
        }
    });
}

API.Auth.logout = function(cb) {
    var self = API.Auth;
    if (self.timeout) clearTimeout (self.timeout);
    delete self.timeout;
    API.delete ("identity","/",function(err,data) {
        self.credentials = {token:"",access:"guest"};
        delete API.headers["X-Auth-Token"];
        cb();
    });
}
