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
    self.View.order_status = status;
    self.refresh();
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

