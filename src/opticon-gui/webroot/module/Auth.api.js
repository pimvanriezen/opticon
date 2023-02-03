API.Auth = new Module.API("Auth");

API.Auth.credentials = {
    token:"",
    access:"gues",
    username:"",
    password:"",
    tenant:"",
    roles:["GUEST"]
}

API.Auth.create = function() {
}

API.Auth.login = function (username, password, cb) {
    var self = API.Auth;
    let svc = "opticon";
    let path = "/login";
    if (conf.auth_method === undefined || conf.auth_method = "unithost") {
        svc = "identity";
        path = "/";
    }
    API.post (svc,path,{username,password}, function (err, data) {
        if (err) {
            cb (false);
            return;
        }
        
        if (data.token) {
            self.credentials.token = data.token;
            if (conf.auth_method == "internal") {
                API.headers["X-Opticon-Token"] = data.token;
                cb (true);
                return;
            }

            API.headers["X-Auth-Token"] = data.token;
            
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
        else {
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
            if (! self.credentials.username) return;
            if (self.credentials.username == "") return;
            self.login (self.credentials.username,
                        self.credentials.password, function(res) {
                if (! res) {
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
