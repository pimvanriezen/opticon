API.Opticon = new Module.API("Opticon");

API.Opticon.create = function() {
}

API.Opticon.hostOverview = function(cb) {
    let tenant = "any"
    if (API.Auth.credentials.access != "admin") {
        tenant = API.Auth.credentials.tenant;
    }
    API.get ("opticon","/"+tenant+"/host/overview", cb);
}
