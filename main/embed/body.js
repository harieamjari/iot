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
var t = document.getElementById("status_table");
t.innerHTML += `
	<tr>
		<td> Tests </td>
		<td> sasa </td>
	</tr>
`;
document.getElementById("statusTab").click();
var date = new Date();
document.getElementById("currentTime").value = date.getFullYear() + "-" +
                                               (date.getMonth() + 1) +
                                               "-" + date.getDate() +
                                               " " + date.getHours() +
                                               ":" + date.getMinutes() +
                                               ":" + date.getSeconds();
