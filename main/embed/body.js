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
function update_id(id) {
  fetch('/' + id).then(response => {
    if (!response.ok) {
      throw new Error('http error');
    }
    return response.text();
  }).then(data => {
    document.getElementById(id).innerHTML = data;
  }).catch(e => {alert("could not fetch " + id);});
}
function update_ids() {
  update_id('status_table');
  update_id('schedule_table');
  
}
function gpio_switch(elmnt, pin, v) {
  const data = {gpio: pin, switchv: v};
  const edata = Object.keys(data)
    .map(key => encodeURIComponent(key) + '=' + encodeURIComponent(data[key]))
    .join('&');
  fetch('/gpio', {method: 'POST', body: edata});
}
if (window.location.pathname === '/') {
  const url_params = new URLSearchParams(window.location.search);
  const tab_param_value = url_params.get('tab');
  if (tab_param_value === null)
    document.getElementById("status_tab").click();
  else if (tab_param_value === 'lighting') {
    document.getElementById("lighting_tab").click();
  }
}
update_status();

var date = new Date();
document.getElementById("currentTime").value = date.getFullYear() + "-" +
                                               (date.getMonth() + 1) +
                                               "-" + date.getDate() +
                                               " " + date.getHours() +
                                               ":" + date.getMinutes() +
                                               ":" + date.getSeconds();
