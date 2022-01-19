$(document).ready(function() {

    let uploadFormData;
    let fileName;

    function getArgs() {
        let args = ""
        for (let param of $(".param")) {
            param = $(param);
            const id = param.attr("id");
            const required = param.attr("required");
            const value = param.val().trim();
            if (required && value === "") {
                param.focus()
                showSnackbar(basicSnackbar, "This field is required")
                return
            }
            if (isNaN(value)) {
                param.focus()
                showSnackbar(basicSnackbar, "Some fields are not numbers")
                return
            }
            args += `${id}=${value}&`
        }
        args = args.slice(0, -1)
        return args
    }


    function loadFile(files) {
        let formData = new FormData();
        for (let i = 0; i < files.length; i++) {
            const file = new File([files[i]], encodeURIComponent(files[i].name), {
                type: files[i].type,
                lastModified: files[i].lastModified,
            });
            formData.append("file" + i, file);
        }
        formData.append("test", "hi")
        fileName = files[0].name;
        $("#file-name").text(fileName);
        uploadFormData = formData;
    }

   $("#analyze").click(async function() {
       if (!uploadFormData || !fileName) {
           return showDialog(okDialog, locale["app_name"], locale["upload_required"]);
       }

       const args = getArgs()
       if (!args) return
       const uploadLink = location.pathname + "?" + args
       const uploadXhr = await request("POST", uploadLink, uploadFormData);
       uploadFormData = undefined;
       if (uploadXhr.status === 200) {
           location.pathname = "/portal/" + fileName;
       } else {
           showSnackbar(basicSnackbar, uploadXhr.responseText);
       }
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


