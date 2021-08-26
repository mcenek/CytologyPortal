$(document).ready(function() {

    let uploadFormData;
    let fileName;
    function loadFile(files) {
        let formData = new FormData();
        for (let i = 0; i < files.length; i++) {
            const file = new File([files[i]], encodeURIComponent(files[i].name), {
                type: files[i].type,
                lastModified: files[i].lastModified,
            });
            formData.append("file" + i, file);
        }
        fileName = files[0].name;
        $("#file-name").text(fileName);
        uploadFormData = formData;
    }

   $("#analyze").click(function() {
       if (!uploadFormData || !fileName) {
           return showDialog(okDialog, locale["app_name"], locale["upload_required"]);
       }

       request("POST", location.pathname, uploadFormData, function(xmlHttpRequest) {
           if (xmlHttpRequest.status === 200) {
               location.pathname = "/portal/" + fileName;
           } else {
               showSnackbar(basicSnackbar, xmlHttpRequest.responseText);
           }
       }, undefined, null);
       uploadFormData = undefined;
   })

    $("#analyze-upload").click(function() {
        $("#analyze-uploadButton").val("");
        $("#analyze-uploadButton").trigger("click");
    });

    $("#analyze-uploadButton").change(function() {
        let files = $(this)[0].files;
        loadFile(files);
    });

});


