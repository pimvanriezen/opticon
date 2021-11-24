// ==========================================================================
// Error dialog for anything that is unrecoverable.
// ==========================================================================
FatalError = new Module.Dialog ("FatalError");

// --------------------------------------------------------------------------
// Constructor
// --------------------------------------------------------------------------
FatalError.create = function() {
    var self = FatalError;
    self.createView({
        title:"Error",
        text:"Text"
    });
}

// --------------------------------------------------------------------------
// Opens the dialog with specified parameters
// --------------------------------------------------------------------------
FatalError.open = function(title,text) {
    var self = FatalError;
    self.View.title = title;
    self.View.text = text;
    self.show();
}

// --------------------------------------------------------------------------
// Handler for the single button. Performs a reload on the page.
// --------------------------------------------------------------------------
FatalError.close = function() {
    var self = FatalError;
    location.reload();
}
