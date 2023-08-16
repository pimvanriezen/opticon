API.Opticon = new Module.API("Opticon");

// Creator
API.Opticon.create = function() {
}

API.Opticon.Host = {
    tcache: {}
}

API.Opticon.Host.getOverview = function(cb) {
    let tenant = "any"
    if (API.Auth.credentials.access != "admin") {
        tenant = API.Auth.credentials.tenant;
    }
    API.get ("opticon","/"+tenant+"/host/overview", cb);
}

API.Opticon.Host.resolveTenant = function(hostuuid, cb) {
    let self = API.Opticon.Host;
    
    if (API.Auth.credentials.access != "admin") {
        cb (API.Auth.credentials.tenant);
        return;
    }
    
    if (self.tcache[hostuuid] !== undefined) {
        cb (self.tcache[hostuuid]);
        return;
    }
    
    API.get ("opticon", "/any/host/"+hostuuid+"/tenant", function (err, res) {
        if ((!err) && res.tenant) {
            self.tcache[hostuuid] = res.tenant;
            cb (res.tenant);
        }
        else {
            cb (null);
        }
    });
}

API.Opticon.Host.getCurrent = function (tenant, host, cb) {
    if (tenant == "any") {
        API.Opticon.Host.resolveTenant (host, function(t) {
            if (t) {
                API.Opticon.Host.getCurrent (t, host, cb);
            }
            else {
                cb (null);
            }
        });
        return;
    }
    
    API.get ("opticon","/"+tenant+"/host/"+host, function (err, res) {
        if (err) cb (null);
        else cb (res);
    });
}

API.Opticon.Host.getExternalData = function (tenant, host, cb) {
    if (tenant == "any") {
        API.Opticon.Host.resolveTenant (host, function(t) {
            if (t) {
                API.Opticon.Host.getExternalData (t, host, cb);
            }
            else {
                cb (null);
            }
        });
        return;
    }
    
    API.get ("opticon","/"+tenant+"/host/"+host+"/externaldata",
             function (err, res) {
        if (err) cb (null);
        else cb (res);
    });
}

API.Opticon.Host.listWatchers = function (tenant, host, cb) {
    if (tenant == "any") {
        API.Opticon.Host.resolveTenant (host, function(t) {
            if (t) {
                API.Opticon.Host.listWatchers (t, host, cb);
            }
            else {
                cb (null);
            }
        });
        return;
    }
    
    API.get ("opticon","/"+tenant+"/host/"+host+"/watcher", function (err, res) {
        if (err) cb (null);
        else cb (res);
    });
}

API.Opticon.Host.remove = function (tenant, host, cb) {
    if (tenant == "any") {
        API.Opticon.Host.resolveTenant (host, function(t) {
            if (t) {
                API.Opticon.Host.remove (t, host, cb);
            }
            else {
                cb (null);
            }
        });
        return;
    }
    
    API.delete ("opticon","/"+tenant+"/host/"+host, function (err, res) {
        if (err) cb (null);
        else cb (res);
    });
}

API.Opticon.Host.getLog = function (tenant, host, cb) {
    if (tenant == "any") {
        API.Opticon.Host.resolveTenant (host, function(t) {
            if (t) {
                API.Opticon.Host.getLog (t, host, cb);
            }
            else {
                cb (null);
            }
        });
        return;
    }
    
    API.get ("opticon","/"+tenant+"/host/"+host+"/log", function (err, res) {
        if (err) cb (null);
        else cb (res);
    });
}

API.Opticon.Host.getGraph = function (tenantid, hostuuid, graph, datum,
                                     timespan, width, cb) {
    var numsamples = Math.floor (timespan/300);
    if (numsamples > 64) numsamples = 64;
    
    API.get ("opticon","/"+tenantid+"/host/"+hostuuid+
             "/graph/"+graph+"/"+datum+"/"+Math.floor(timespan)+
             "/"+numsamples, function (err, res) {
        if (err) cb (null);
        else cb (res);       
    });
}

API.Opticon.Tenant = {}

API.Opticon.Tenant.list = function (cb) {
    API.get ("opticon","/", function (err,res) {
        if (err) cb (null);
        cb (res.tenant);
    });
}

API.Opticon.Tenant.getQuota = function(tenant, cb) {
    API.get ("opticon", "/"+tenant+"/quota", function (err, res) {
        if (err) cb (null);
        cb (res);
    });
}

API.Opticon.Tenant.getMeta = function(tenant, cb) {
    API.get ("opticon", "/"+tenant+"/meta", function (err, res) {
        if (err) cb (null);
        cb (res);
    });
}
