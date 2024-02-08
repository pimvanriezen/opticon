// ==========================================================================
// Root of the API namespace. Functions for interacting with the backend.
// ==========================================================================
API = {
    headers:{},
    urls:{
        registry:conf.url.unithost,
        opticon:conf.url.opticon
    },
    wssurls:{},
    disabled:false
};

// --------------------------------------------------------------------------
// Serialize a key/value object into a query string
// --------------------------------------------------------------------------
API.serialize = function(obj) {
  var str = [];
  for(var p in obj)
    if (obj.hasOwnProperty(p)) {
      str.push(encodeURIComponent(p) + "=" + encodeURIComponent(obj[p]));
    }
  return str.join("&");
}

// --------------------------------------------------------------------------
// Generate an API url.
// --------------------------------------------------------------------------
API.mkURL = function(service,path,cb) {
    if (API.disabled) return;
    if (API.urls[service] != undefined) {
        cb (API.urls[service]+path);
        return;
    }
    
    console.log ("[API] Resolving service <" + service + ">");
    
    let url = API.urls.registry;
    if (! url) {
        console.log ("[API] Registry not found");
        cb (null);
        return;
    }
    
    API.urls["registry"] = url;
    API.get ("registry","/service",function(err,res) {
        if (res[service] != undefined) {
            API.urls[service] = res[service].external_url;
            cb (API.urls[service]+path);
            return;
        }
        console.log ("[API] Service `" + service + "` not in registry");
        cb (null);
    });
}

API.getWSURL = function(service,cb) {
    if (API.disabled) return;
    if (API.wssurls[service] != undefined) {
        cb (API.wssurls[service]);
        return;
    }
    
    API.get ("registry","/service",function(err,res) {
        if (res[service] != undefined) {
            API.wssurls[service] = res[service].external_wsurl;
            cb (API.wssurls[service]);
            return;
        }
        console.log ("[API] WSS service not in registry");
        cb (null);
    });
}

API.getServiceTag = function(cb) {
    if (API.disabled) return;
    API.get ("registry","/tag",function(err,res) {
        if (res.tag) cb (res.tag);
        else cb (null);
    });
}

// --------------------------------------------------------------------------
// Make a call to the backend.
// --------------------------------------------------------------------------
API.call = function(service,path,method,indata,cb,silent,retry) {
    if (API.disabled) return;
    let dtype = (method=="BLOB") ? "text" : "json";
    method = (method=="BLOB") ? "GET" : method;

    var fcomplete = function(xhr, status) {
        if (! silent) {
            if (xhr.status==500) {
                var extramsg = "";
                if (dtype == "json") {
                    var robj = xhr.responseJSON;
                    console.log ("[API] 500 error",robj);
                    if (robj.stack) {
                        var st = robj.stack.split('\n');
                        for (var i=0; i<st.length; ++i) {
                            console.log (st[i]);
                        }
                        extramsg = "<br><br>" + st.slice(0,3).join('<br>');
                    }
                    else if (robj.error) {
                        if (typeof (robj.error) == "Object") {
                            extramsg = JSON.stringify(robj.error);
                        }
                        else extramsg = ""+robj.error;
                        extramsg = extramsg.replace (/</g,'&lt;');
                        extramsg = "<br><br>" + extramsg;
                    }
                }

                FatalError.open ("Fatal error communicating through API",
                                 "The service returned a 500 status, indicating "+
                                 "an irrecoverable error. <br><br>"+
                                 method+" "+service+":"+path+extramsg);
                
                cb (500,{});
                return;
            }
            if (xhr.status==0 || xhr.status==502 || xhr.status==504) {
                if (! retry) retry = 0;
                retry = retry+1;
                if (retry < 5) {
                    setTimeout (function() {
                        API.call (service,path,method,indata,cb,silent,retry);
                    }, 1000*retry);
                }
                else {
                    FatalError.open ("Network Error",
                                     "Error communicating with the '"+
                                      service+"' API. "+
                                      "path: "+path);
                    cb ("timeout",{});
                    return;
                }
                return;
            }
        }
        if (xhr.status==403) {
            if (! LoginScreen.isOpen) {
                NormalError.open ("Authorization Error","The action you tried "+
                                  "to perform was not allowed by the system.");
            }
            cb (status, {"error":status});
        }
        if (status=="success") {
            cb (null, (dtype=="text")?xhr.responseText:xhr.responseJSON);
        }
        else {
            console.log ("[API] error",xhr,status)
            cb (status, {"error":status,"status":xhr.status})
        }   
    }

    API.mkURL(service,path,function(outurl) {
        if (! outurl) {
            cb ("Unknown Service",{"error":"Unknown Service"});
            return;
        }
        if (indata && method != "GET") {
            console.log ("[API] " + method + " " + outurl);

            $.ajax({
                url: outurl,
                headers: API.headers,
                datatype: dtype,
                data: JSON.stringify(indata),
                contentType: "application/json",
                method: method,
                complete: fcomplete
            });
        }
        else {
            var callurl = outurl;
            if (indata) callurl = callurl + "?" + API.serialize(indata);
            console.log ("[API] " + method + " " + callurl);
            $.ajax({
                url: callurl,
                headers: API.headers,
                datatype: "json",
                method: method,
                complete: fcomplete
            })
        }
    });
}

// --------------------------------------------------------------------------
// Wrapper for POST requests
// --------------------------------------------------------------------------
API.post = function(service,path,data,cb) {
    API.call(service,path,"POST",data,cb);
}

// --------------------------------------------------------------------------
// Wrapper for GET requests
// --------------------------------------------------------------------------
API.get = function(service,path,query,cb) {
    if (! cb) {
        cb = query;
        query = null;
    }
    API.call(service,path,"GET",query,cb);
}

API.get.blob = function(service,path,query,cb) {
    if (! cb) {
        cb = query;
        query = null;
    }
    API.call(service,path,"BLOB",query,cb);
}

// --------------------------------------------------------------------------
// Silent version of the GET call. Will not spawn hell when running into
// timeouts or other shenanigans; used by EventListener.
// --------------------------------------------------------------------------
API.get.silent = function(service,path,query,cb) {
    if (! cb) {
        cb = query;
        query = null;
    }
    API.call(service,path,"GET",query,cb,true);
}

// --------------------------------------------------------------------------
// Wrapper for PUT requests
// --------------------------------------------------------------------------
API.put = function(service,path,data,cb) {
    API.call(service,path,"PUT",data,cb);
}

// --------------------------------------------------------------------------
// Wrapper for PATCH requests
// --------------------------------------------------------------------------
API.patch = function(service,path,data,cb) {
    API.call(service,path,"PATCH",data,cb);
}

// --------------------------------------------------------------------------
// Wrapper for DELETE requests
// --------------------------------------------------------------------------
API.delete = function(service,path,query,cb) {
    if (! cb) {
        cb = query;
        query = null;
    }
    API.call(service,path,"DELETE",query,cb);
}

API.shutDown = function() {
    API.disabled = true;
}
