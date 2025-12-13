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
  }).catch(e => {
//	  alert("could not fetch " + id);
  });
}
function update_checkbox(id) {
  fetch('/gpio_level?gpio=' + id).then(response => {
    if (!response.ok) {
      throw new Error('http error');
    }
    return response.text();
  }).then(data => {
    document.getElementById('gpio' + id).checked = data == '0' ? false : true;
  }).catch(e => {alert("could not fetch " + id);});
}
function gpio_switch(elmnt, pin) {
  const v = document.getElementById("gpio" + pin).checked === false ? 'off' : 'on';
  const data = {gpio: pin, value: v};
  const edata = Object.keys(data)
    .map(key => encodeURIComponent(key) + '=' + encodeURIComponent(data[key]))
    .join('&');
  fetch('/gpio', {method: 'POST', body: edata});
}
function update_datetime(elmnt) {
  var e = document.getElementById("setting_datetime");
  const data = {datetime: e.value};
  const edata = Object.keys(data)
    .map(key => encodeURIComponent(key) + '=' + encodeURIComponent(data[key]))
    .join('&');
  fetch('/update_datetime', {method: 'POST', body: edata});
  window.location.reload();
}
function add_schedule(){

}
setInterval(update_id, 1000, 'status_table');
update_id('schedule_table');
update_id('quote');
update_checkbox('25');
update_checkbox('26');
update_checkbox('27');
update_checkbox('33');
// attics
update_checkbox('22');
update_checkbox('23');
const url_params = new URLSearchParams(window.location.search);
const tab_param_value = url_params.get('tab');
if (tab_param_value === 'lighting')
  document.getElementById("lighting_button").click();
else {
  document.getElementById("status_button").click();
}
