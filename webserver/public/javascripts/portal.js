getRequest(location.pathname + "/export.json", function(xmlHttpRequest) {
    if (xmlHttpRequest.status == 200) {
        const export_data = JSON.parse(xmlHttpRequest.response);
        const nucleiCytoRatios = export_data["nucleiCytoRatios"];
        const thumbnails = export_data["thumbnails"];

        const chart_data = [];
        const sortedNucleiCytoRatios = [...nucleiCytoRatios].sort();
        for (const i in sortedNucleiCytoRatios) {
            chart_data.push({x: i, y: sortedNucleiCytoRatios[i]});
        }

        $(document).ready(function() {
            let ctx = $("#myChart");
            $("#lower-ratio").val("0");
            $("#lower-ratio").focus();
            $("#lower-ratio").blur();

            $("#upper-ratio").val(sortedNucleiCytoRatios[sortedNucleiCytoRatios.length - 1]);
            $("#upper-ratio").focus();
            $("#upper-ratio").blur();

            let myChart = new Chart(ctx, {
                type: 'scatter',
                data: {
                    datasets: [{
                        label: 'Cyto to Nuclei Ratios',
                        data: chart_data,
                        backgroundColor: 'rgb(255, 99, 132)'
                    }],
                },
                options: {
                    scales: {
                        x: {
                            type: 'linear',
                            position: 'bottom'
                        }
                    }
                }
            });



        });

        let shownThumbnails;

        let page = 0;

        const thumbnailsPerPage = 20;
        function displayThumbnails(page) {
            $("#thumbnails").empty();
            for (let i = 0; i < Math.min(thumbnailsPerPage, shownThumbnails.length); i++) {
                const thumbnail = shownThumbnails[i + thumbnailsPerPage * page];
                $("#thumbnails").append(`<img src="${location.pathname + "/thumbnails/" + thumbnail}" loading="lazy">`);
            }
        }

        $("#show-thumbnails").click(function() {
            $("#thumbnails-card").show();
            page = 0;
            shownThumbnails = [];
            for (const i in thumbnails) {
                const ratio = nucleiCytoRatios[i];
                if (ratio >= parseFloat($("#lower-ratio").val()) && ratio <= parseFloat($("#upper-ratio").val())) {
                    shownThumbnails.push(thumbnails[i]);
                }
            }
            displayThumbnails(page)
        });


        $("#thumbnails-prev").click(function() {
            if (page <= 0) return;
            page--;
            displayThumbnails(page);
        });

        $("#thumbnails-next").click(function() {
            if (page + 1 >= shownThumbnails.length / thumbnailsPerPage) return;
            page++;
            displayThumbnails(page);
        });




    }
})



