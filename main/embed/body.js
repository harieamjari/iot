function openPage(pageName, elmnt) {
  // alert(pageName);
  var tabcontents = document.getElementsByClassName("tabcontent");
  for (var i = 0; i < tabcontents.length; i++)
    tabcontents[i].style.display = "none";
  document.getElementById(pageName).style.display = "block";
}
function onClick(elmnt) {
  alert("new value " + elmnt.checked + " " + elmnt.id);
}
function onChange(elmnt) { alert(elmnt.id + " was changed"); }
function updateStatus(elmnt) {}
fetch('/status').then(response => {
  document.getElementById("status_table").innerHTML = response;
});

const url_params = new URLSearchParams(window.location.search);
const tab_param_value = url_params.get('tab');
if (tab_param_value === null)
  document.getElementById("status_tab").click();
else if (tab_param_value === 'lighting') {
  document.getElementById("lighting_tab").click();
}

var date = new Date();
document.getElementById("currentTime").value = date.getFullYear() + "-" +
                                               (date.getMonth() + 1) +
                                               "-" + date.getDate() +
                                               " " + date.getHours() +
                                               ":" + date.getMinutes() +
                                               ":" + date.getSeconds();
