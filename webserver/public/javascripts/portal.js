getRequest(location.pathname + "/export.json", function(xmlHttpRequest) {
    if (xmlHttpRequest.status === 200) {

        const export_data = JSON.parse(xmlHttpRequest.response);
        const nucleiCytoRatios = export_data["nucleiCytoRatios"];
        const thumbnails = export_data["thumbnails"];


        const sorted = [...Array(thumbnails.length).keys()];
        sorted.sort(function(first, second) {
            return nucleiCytoRatios[first] - nucleiCytoRatios[second];
        })

        const chart_data = [];
        const sortedNucleiCytoRatios = [...nucleiCytoRatios].sort();


        for (const i in thumbnails) {
            chart_data.push({x: i, y: nucleiCytoRatios[sorted[i]]});
        }

        let myChart;
        $(document).ready(function() {
            let ctx = $("#myChart");
            $("#lower-ratio").val("0");
            $("#lower-ratio").focus();
            $("#lower-ratio").blur();

            $("#upper-ratio").val(nucleiCytoRatios[sorted[thumbnails.length - 1]]);
            $("#upper-ratio").focus();
            $("#upper-ratio").blur();

            myChart = new Chart(ctx, {
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
        let shownThumbnailsIdx;

        let page = 0;

        const thumbnailsPerPage = 20;
        function displayThumbnails(page) {
            $("#thumbnails").empty();

            let shownThumbnailsData = [];
            for (let i = 0; i < Math.min(thumbnailsPerPage, shownThumbnails.length); i++) {
                const thumbnail = shownThumbnails[i + thumbnailsPerPage * page];
                const nucleiCytoRatio = nucleiCytoRatios[sorted[shownThumbnailsIdx[i + thumbnailsPerPage * page]]];
                shownThumbnailsData.push({x: shownThumbnailsIdx[i + thumbnailsPerPage * page], y: nucleiCytoRatio});
                $("#thumbnails").append(`<img class="thumbnail" src="${location.pathname + "/thumbnails/" + thumbnail}" loading="lazy">`);
            }

            console.log(shownThumbnailsData)

            const shownLabel = "Shown Thumbnails";
            if (myChart.data.datasets[0].label == shownLabel) {
                myChart.data.datasets.shift();
            }
            myChart.data.datasets.splice(0, 0, {
                label: shownLabel,
                data: shownThumbnailsData,
                backgroundColor: 'rgb(0, 99, 132)'
            });
            myChart.update();
        }



        $("#show-thumbnails").click(function() {
            $("#thumbnails-card").show();
            page = 0;
            shownThumbnails = [];
            shownThumbnailsIdx = [];
            for (const i in thumbnails) {

                const ratio = nucleiCytoRatios[sorted[i]];
                if (ratio >= parseFloat($("#lower-ratio").val()) && ratio <= parseFloat($("#upper-ratio").val())) {
                    shownThumbnails.push(thumbnails[sorted[i]]);
                    shownThumbnailsIdx.push(i);
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


        $("#content").show();

    } else if (xmlHttpRequest.status == 404) {

    }
})



