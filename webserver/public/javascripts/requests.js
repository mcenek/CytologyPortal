function request(method, url, data, authorization, contentType) {
    if (!contentType) {
        contentType = "application/json";
        if (typeof data == "object") data = JSON.stringify(data);
    }
    const headers = {}
    if (authorization) {
        headers["Authorization"] = authorization
    }
    return new Promise((resolve, reject) => {
        $.ajax({
            url: url,
            type: method,
            data: data,
            headers: headers,
            contentType: contentType,
            complete: (jqXHR) => {
                try {
                    jqXHR.responseJSON = JSON.parse(jqXHR.responseText);
                } catch {}
                resolve({
                    status: jqXHR.status,
                    statusText: jqXHR.statusText,
                    responseText: jqXHR.responseText,
                    responseJSON: jqXHR.responseJSON
                })
            }
        });
    })

}

async function getRequest(url, authorization) {
    return request("GET", url, null, authorization);
}

function headRequest(url, authorization) {
    return request("HEAD", url, null, authorization);
}

function postRequest(url, data, authorization) {
    return request("POST", url, data, authorization);
}

function putRequest(url, data, authorization) {
    return request("PUT", url, data, authorization);
}

function patchRequest(url, data, authorization) {
    return request("PATCH", url, data, authorization);
}

function deleteRequest(url, data, authorization) {
    return request("DELETE", url, data, authorization);
}

function getQueryVariable(variable) {
    let query = window.location.search.substring(1);
    let vars = query.split('&');
    for (let i = 0; i < vars.length; i++) {
        let pair = vars[i].split('=');
        if (decodeURIComponent(pair[0]) === variable) {
            return decodeURIComponent(pair[1]);
        }
    }
    return null;
}