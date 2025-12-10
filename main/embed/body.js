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
function update_ids() {
  update_id('status_table');
  update_id('schedule_table');

//						<td><input type="checkbox" onclick="gpio_switch(this, '25')" id="lroom" checked="true"></td>
//					</tr>
//					<tr>
//						<td>Family room</td>
//						<td><input type="checkbox" onclick="gpio_switch(this, '26')" id="froom" checked="true"></td>
//					</tr>
//					<tr>
//						<td>Dining room</td>
//						<td><input type="checkbox" onclick="gpio_switch(this, '27')" id="droom" checked="true"></td>
//					</tr>
//					<tr>
//						<td>Bathroom</td>
//						<td><input type="checkbox" onclick="gpio_switch(this, '33')" id="broom" checked="true"></td>
//  
}
function gpio_switch(elmnt, pin) {
  const v = document.getElementById("gpio" + pin).checked === false ? 'off' : 'on';
  const data = {gpio: pin, value: v};
  const edata = Object.keys(data)
    .map(key => encodeURIComponent(key) + '=' + encodeURIComponent(data[key]))
    .join('&');
  fetch('/gpio', {method: 'POST', body: edata});
    //.then(data => {document.getElementById("gpio" + pin).checked = (v === 'true' ? 'false' : 'true');});
  //location.reload();
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
update_ids();
update_checkbox('25');
update_checkbox('26');
update_checkbox('27');
update_checkbox('33');

var date = new Date();
document.getElementById("currentTime").value = date.getFullYear() + "-" +
                                               (date.getMonth() + 1) +
                                               "-" + date.getDate() +
                                               " " + date.getHours() +
                                               ":" + date.getMinutes() +
                                               ":" + date.getSeconds();
