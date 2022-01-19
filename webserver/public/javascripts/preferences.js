let preferenceFieldValues;

$(document).ready(function() {
    if (!preferences) return showSnackbar(basicSnackbar, "Preferences error");
    let table = $("#preferences").find("tbody");
    let tableRows = $("#preferences").find("tr:not(.first-row)");
    tableRows.remove();

    for (let key in preferences) {
        if (!preferences.hasOwnProperty(key)) continue;
        let value = preferences[key];

        //Hide expired entries (must implement data validation for end date so that end date > Date.now())
        //if (info.end !== null && info.end <= Date.now()) continue;
        table.append(getPrefenecesRowHTML(key, value));
    }

    $(".preferences.value").blur(updateField);
});

$(document).keypress(function(e) {
    let key = e.which;
    if (key === 13) {
        $(document.activeElement).blur();
    }
});

function getPrefenecesRowHTML(key, value) {
    let html = `<tr name=${key}>` +
        `<td><input class='key preferences first-column' disabled name=${key} type='text' autocomplete='off' autocapitalize='none' value='${key}''></td>` +
        `<td><input class='value preferences' name=${key} type='text' autocomplete='off' autocapitalize='none' value='${value}' placeholder='${"No value"}'></td>` +
        `</tr>`;
    return html;
}

function updateCallback(xmlHttpRequest, event) {
    if (xmlHttpRequest.status === 0) {
        showDialog(okDialog, locale.app_name, locale.session_expired, {
            "close": function () {
                location.reload()
            }
        });
    } else if (xmlHttpRequest.status !== 200) {
        showSnackbar(basicSnackbar, xmlHttpRequest.responseText);
        let field = $(event.target);
        let key = field.attr("name");
        let oldValue = preferences[key];
        field.val(oldValue);
    }
}

async function updateField(event) {
    let field = $(event.target);
    let key = field.attr("name");
    let value = field.val();

    let data = {};
    data[key] = value;

    let validated = true;

    if (validated !== false) {
        const uploadXhr = await postRequest(location.pathname, JSON.stringify(data));
        updateCallback(uploadXhr, event);
    } else {
        let oldValue = preferences[key];
        field.val(oldValue);
    }
}