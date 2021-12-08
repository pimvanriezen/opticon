// ==========================================================================
// Implementation of the CustomerOrder Page module.
// ==========================================================================
ServerList = new Module.Page("ServerList", "/Server");

// --------------------------------------------------------------------------
// Constructor called when module is loaded.
// --------------------------------------------------------------------------
ServerList.create = function () {
    var self = ServerList;
    self.createView({
        servers: [],
        haveselection: false,
        selected: "",
        empty: false,
        server_status: "ALERT",
    });
}

// --------------------------------------------------------------------------
// Stores the loaded parent customer's id.
// --------------------------------------------------------------------------
ServerList.selectedObject = null;

// --------------------------------------------------------------------------
// Activate method. Loads customer from backend API.
// --------------------------------------------------------------------------
ServerList.activate = function (argv) {
    var self = ServerList;
    self.View.haveselection = false;
    self.View.selected = "";
    self.selectedObject = null;
    
    self.View.servers = {
        "1a7f7412-d13c-4af8-ae7c-defa899bcf59":{
            hostname:"server1",
            ip:"10.1.1.5",
            status:"OK",
            pcpu:1.2,
            loadavg:0.4,
            rtt:11.2
        },
        "b77831c8-afab-46bd-9f6d-8e72beda4803":{
            hostname:"server2",
            ip:"10.1.1.6",
            status:"OK",
            pcpu:11.5,
            loadavg:0.9,
            rtt:8.5
        }
    }
    
    /*    
    
    API.Order.list({status: self.View.order_status}, function (err, res) {
        if (err) res = [];
        self.setOrders(res);
    });
    
    */
    
    self.show();
}

// --------------------------------------------------------------------------
// Background task.
// --------------------------------------------------------------------------
ServerList.refresh = function () {
    var self = ServerList;
    
    /*
    API.Order.list({
        status: self.View.order_status
    }, function (err, res) {
        if (res) {
            self.setOrders(res);
        }
    });
    
    */
}

ServerList.switchTab = function (status) {
    let self = ServerList;
    self.View.selected = "";
    self.View.haveselection = false;
    self.View.server_status = status;
    self.refresh();
}

ServerList.statusClass = function (server) {
    if (server.status == "OK") return "status green";
    if (server.status == "WARN") return "status orange";
    if (server.status == "ALERT") return "status red";
    return "status grey";
}

// --------------------------------------------------------------------------
// Click handler for result row.
// --------------------------------------------------------------------------
ServerList.rowClick = function (data, event) {
    $(".uOrderList li.selected").removeClass("selected");
    $(".uOrderList li#orderList-" + data.id).addClass("selected");
    var self = ServerList;
    setTimeout(function () {
        self.Vidi.lock();
        self.View.selected = data.id;
        self.View.haveselection = true;
        self.selectedObject = data;
        self.Vidi.unlock();
    }, 50);
    if (event) event.stopPropagation();
    return false;
}

// --------------------------------------------------------------------------
// Handler for a double click on the row.
// --------------------------------------------------------------------------
ServerList.rowDoubleClick = function(order) {
    let self = ServerList;
}

// --------------------------------------------------------------------------
// Click handler for empty space.
// --------------------------------------------------------------------------
ServerList.deSelect = function () {
    var self = ServerList;
    self.View.selected = "";
    self.View.selectedObject = null;
    self.View.haveselection = false;
    return false;
}

