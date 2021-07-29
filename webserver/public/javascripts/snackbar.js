
String.format = function(format) {
    let args = Array.prototype.slice.call(arguments, 1);
    return format.replace(/{(\d+)}/g, function(match, number) {
        return typeof args[number] != 'undefined'
            ? args[number]
            : match
            ;
    });
};

let snackbar_;
let basicSnackbar = "<div id=\"snackbar\" class=\"mdc-snackbar\"> <div class=\"mdc-snackbar__surface\"> <div class=\"mdc-snackbar__label\" role=\"status\" aria-live=\"polite\">{0}</div> </div> </div>";
let actionSnackbar = "<div id=\"snackbar\" class=\"mdc-snackbar\"> <div class=\"mdc-snackbar__surface\"> <div class=\"mdc-snackbar__label\" role=\"status\" aria-live=\"polite\">{0}</div> <div class=\"mdc-snackbar__actions\"> <button type=\"button\" class=\"mdc-button mdc-snackbar__action\">{1}</button> </div> </div> </div>";

let isOpen = false;

let showSnackbar = function(type, body, actionText, actionFunction) {

    if (snackbar_ !== undefined && isOpen) {
        snackbar_.close("reopen");

    } else {
        $("body").append(String.format(type, body, actionText));
        snackbar_ = new mdc.snackbar.MDCSnackbar(document.querySelector('#snackbar'));
        snackbar_.open();
        isOpen = true;
    }


    snackbar_.listen('MDCSnackbar:closed', function() {
        isOpen = false;
        $("#snackbar").remove();

        if (event.detail.reason === "action") {
            if (actionFunction !== undefined) actionFunction();
        }

        if (event.detail.reason === "reopen") {
            showSnackbar(type, body, actionText, actionFunction);

        }
    });

    return snackbar_;
};