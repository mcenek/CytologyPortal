function sortAscending() {
    folderContents.sort((a, b) => {
        if (a.type === b.type) {
            const sortNameA = (a.decrypted_name) ? a.decrypted_name : a.name;
            const sortNameB = (b.decrypted_name) ? b.decrypted_name : b.name;
            return sortNameA.localeCompare(sortNameB)
        } else {
            if (a.type === "directory") return -1;
            return 1;
        }
    })
}

sortAscending();

$(document).ready(function() {
    folderContents = folderContents.filter(function (file) {
        return !getDisplayName(file).startsWith(".");
    });

    let filesPathSplit = path.split("/");

    let folderName;

    if (path.trim() === "") {
        folderName = baseUrl.charAt(1).toUpperCase() + baseUrl.substring(2);
    } else folderName = getPathDisplayName(filesPathSplit.length - 1);

    let currentPath = baseUrl;

    $(".mdc-drawer__title").text(folderName);

    let selectedItem;

    if (filesPathSplit.length === 0) {
        $("#back").hide();
    }

    if (filesPathSplit.length > 3) {
        $("#navigation-bar").append("<td><div style='margin-top: 10px' class=\"navigation-arrow material-icons\">chevron_right</div></td>");
        $("#navigation-bar").append("<td><button class='mdc-menu-surface--anchor' id='path-overflow-button' style='font-size: 15px; margin-top: 7px; margin-left: 5px; border: none; outline: none; background-color: transparent'><h4>...</h4><div id='path-overflow-menu' class=\"mdc-menu mdc-menu-surface\"> <ul id='path-overflow-list' class=\"mdc-list\" role=\"menu\" aria-hidden=\"true\" aria-orientation=\"vertical\" tabindex=\"-1\"> </ul> </div></button></td>");
        const pathOverflowMenu = new mdc.menu.MDCMenu(document.querySelector('#path-overflow-menu'));
        let currentPathOriginal = currentPath;

        pathOverflowMenu.listen("MDCMenu:selected", function() {
            let currentPathCopy = currentPathOriginal;
            for (let i = 0; i <= event.detail.index; i++) {
                currentPathCopy += "/" + encodeURIComponent(filesPathSplit[i]);
            }

            window.location.href = currentPathCopy;
        });

        $("#path-overflow-button").click(function(event) {
            if (!$(event.target).is("button") && !$(event.target).is("h4") ) return;
            pathOverflowMenu.open = !pathOverflowMenu.open;
        });
    }

    for (let directoryIndex in filesPathSplit) {
        let directory = filesPathSplit[directoryIndex];
        currentPath += "/" + encodeURIComponent(directory);
        if (directory.trim() === "") continue;
        if (filesPathSplit.length > 3 && directoryIndex < filesPathSplit.length - 2) {
            $("#path-overflow-list").append("<li class=\"mdc-list-item\" role=\"menuitem\"> <span class=\"mdc-list-item__text\">" + directory + "</span> </li>");
            continue;
        }
        $("#navigation-bar").append(`<td><div style='margin-top: 10px' class=\"navigation-arrow material-icons\">chevron_right</div></td>`);
        $("#navigation-bar").append(`<td><button onclick="window.location.href = \`${currentPath}\`;" style='font-size: 16px; margin-top: 7px; margin-left: 5px; border: none; outline: none; background-color: transparent'><h4>${getPathDisplayName(directoryIndex)}</h4></button></td>`);
    }

    $("#navigation-bar").append(`<td hidden id='actionButtons' style='position: absolute; right: 7px;'><button id='download' class=\"mdc-icon-button material-icons\">cloud_download</button><button id='delete' class=\"mdc-icon-button material-icons\">delete</button></td>`);

    let iconButtons = document.getElementsByClassName('mdc-icon-button');
    for (let i = 0; i < iconButtons.length; i++) {
        new mdc.ripple.MDCRipple(iconButtons[i]).unbounded = true;
    }

    let files = {};


    for (let fileIndex in folderContents) {
        if (folderContents.hasOwnProperty(fileIndex)) {
            let file = folderContents[fileIndex];
            if (file.name === "..") {
                let backButton = $("#back");
                backButton.click(function() {
                    window.open(location.pathname + "/..", "_self");
                });
                backButton.prop("hidden", false);
                continue;
            }

            file.size = file.size.replace(".00", "");
            file.date = file.date.split(".");
            let month = file.date[0];
            file.date[0] = file.date[1];
            file.date[1] = month;
            file.date = file.date.join("/");
            let icon = "subject";
            if (file.type === "directory") {
                file.size = "----";
                icon = "folder";
            }

            files[fileIndex.toString()] = file;

            let fileName = getDisplayName(file);
            $("#files").append(`<tr class='underlinedTR file' name='${fileIndex}'><td><span class='file-icons material-icons'>${icon}</span></td><td><p>${fileName}</p></td><td><p>${file.size.toUpperCase()}</p></td><td><p>${file.date}</p></td></tr>`);

        }
    }

    $(".file").click(function() {
        fileClick(this);
    });

    $(".file").dblclick(function() {
        fileDblClick(this)
    });

    $(document).keydown(function(e) {
        switch (e.keyCode) {
            case 38:
                $(".file").css("background-color", "");
                if (selectedItem === undefined) {
                    $(".file").eq(0).css("background-color", "#e6e6e6");
                    selectedItem = $(".file").get(0);
                    $("#actionButtons").show()
                } else {
                    let fileId = parseInt($(selectedItem).attr("name"));
                    $(".file").eq((fileId - 1) % $(".file").length).css("background-color", "#e6e6e6");
                    selectedItem = $(".file").get((fileId - 1) % $(".file").length);

                }
                break;

            case 40:
                $(".file").css("background-color", "");
                if (selectedItem === undefined) {
                    $(".file").eq(0).css("background-color", "#e6e6e6");
                    selectedItem = $(".file").get(0);
                    $("#actionButtons").show()
                } else {
                    let fileId = parseInt($(selectedItem).attr("name"));
                    $(".file").eq((fileId + 1) % $(".file").length).css("background-color", "#e6e6e6");
                    selectedItem = $(".file").get((fileId + 1) % $(".file").length);
                }
                break;
        }
    });

    $(document).keypress(function(e) {
        switch (e.which) {
            case 13:
                fileDblClick();
        }
    });

    $(document).click(function (e) {

        if (!$(".file").is(e.target) && $(".file").has(e.target).length === 0) {

            setTimeout(function() {
                selectedItem = undefined;
                $(".file").css("background-color", "");
                $("#actionButtons").hide()
            }, 0);
        }
    });


    $("#download").click(function () {
        let fileId = $(selectedItem).attr("name");
        let fileLink = encodeURIComponent(files[fileId].name);
        window.location.href = [location.pathname, fileLink].join("/") + "?download";
    });

    $("#download-current-dir").click(function () {
        window.location.href = location.pathname + "?download";
    });

    $("#delete").click(function () {
        let fileId = $(selectedItem).attr("name");
        let fileName = getDisplayName(files[fileId])
        let fileLink = encodeURIComponent(files[fileId].name);
        const prompt = locale.confirm_delete.replace("{0}", fileName);
        showDialog(yesNoDialog, locale.app_name, prompt, {
            "yes": function () {
                deleteRequest([location.pathname, fileLink].join("/"), null, function (xmlHttpRequest) {
                    if (xmlHttpRequest.status === 200) {
                        folderContents.splice(fileId, 1);
                        reload();
                    } else {
                        const body = locale.error_deleting.replace("{0}", fileName);
                        showSnackbar(basicSnackbar, body);
                    }
                });
            }
        });
    });


    $("#share").click(function () {
        let fileId = $(selectedItem).attr("name");
        let fileLink = encodeURIComponent(files[fileId].name);
        share({"data": {"filePath": [location.pathname, fileLink].join("/")}});
    });

    // Drag enter
    $("html").on('dragenter', function (e) {

    });



    $("html").on("dragover", function(e) {
        e.preventDefault();
        e.stopPropagation();
    });

    $("html").on("drop", function(e) {
        const files = e.originalEvent.dataTransfer.files;
        uploadFiles(files);
        e.preventDefault(); e.stopPropagation();
    });



    function fileClick(file) {
        $(".file").css("background-color", "");
        $(file).css("background-color", "#e6e6e6");
        $("#actionButtons").show();
        selectedItem = file;
    }

    function fileDblClick() {
        let fileId = $(selectedItem).attr("name");
        let file = files[fileId];
        let link = [location.pathname, encodeURIComponent(file.name)].join("/");
        if (file.type !== "directory") link += "?view";
        window.location = link;
    }

    function reload() {
        $(".file").remove();
        files = {};
        
        for (let fileIndex in folderContents) {
            if (folderContents.hasOwnProperty(fileIndex)) {
                let file = folderContents[fileIndex];

                let icon = "subject";
                if (file.type === "directory") {
                    file.size = "----";
                    icon = "folder";
                }

                files[fileIndex.toString()] = file;

                let fileName = getDisplayName(file);
                $("#files").append(`<tr class='underlinedTR file' name='${fileIndex}'><td><span class='file-icons material-icons'>${icon}</span></td><td><p>${fileName}</p></td><td><p>${file.size.toUpperCase()}</p></td><td><p>${file.date}</p></td></tr>`);

            }
        }

        $(".file").click(function() {
            fileClick(this);
        });
        $(".file").dblclick(function() {
            fileDblClick(this)
        });

    }

});


function getDisplayName(fileInfo) {
    return fileInfo.decrypted_name ? fileInfo.decrypted_name : fileInfo.name
}

function getPathDisplayName(index) {
    return (path_decrypted) ? path_decrypted.split("/")[index] : path.split("/")[index];
}

function deselectAll() {

}