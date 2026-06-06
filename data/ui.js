/*
 * This file is part of the esp8266 web interface
 *
 * Copyright (C) 2018 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

var ui = {

    // The API endpoint to query to get firmware release available in Github
	githubFirmwareReleaseURL: 'https://api.github.com/repos/jsphuebner/stm32-sine/releases',

  //Handle for auto refresh interval
  autoRefreshHandle: 0,

	// temp variable to store updates from Parameter Database
	paramUpdates: "",

	// Status of visibility of parameter categories. E.g. Motor, Inverter. true = visible, false = not visible.
	categoryVisible: {},

	shrinkNavbar: function() {
		document.getElementById("navbar").style.width = "80px";
		var cw = document.getElementById("content-wrapper");
		cw.style.left = "80px";
		cw.style.width = "calc(100% - 80px)";
		var logo = document.getElementById("logo");
		logo.style.width = "80px";
		logo.style.height = "50px";
		// buttons
		var buttons = document.getElementsByClassName("buttonimg");
		for ( let i = 0; i < buttons.length; i++ ) {
			console.log("button ", i, " ", buttons[i]);
			buttons[i].style.width = "60px";
		}
		// hide toggles, version box
		var itemsToHideOnSmallScreen = document.getElementsByClassName("small-screen-hide");
		for ( let i = 0; i < itemsToHideOnSmallScreen.length; i++ ) {
			console.log("item ", i, " ", itemsToHideOnSmallScreen[i]);
			itemsToHideOnSmallScreen[i].style.display = "none";
		}
	},

	growNavbar: function() {
		document.getElementById("navbar").style.width = "180px";
		var cw = document.getElementById("content-wrapper");
		cw.style.left = "180px";
		cw.style.width = "calc(100% - 180px)";
		var logo = document.getElementById("logo");
		logo.style.width = "180px";
		logo.style.height = "100px";
		// buttons
		var buttons = document.getElementsByClassName("buttonimg");
		for ( let i = 0; i < buttons.length; i++ ) {
			buttons[i].style.width = "24px";
		}
		// show toggles, version box
		var itemsToShowOnBigScreen = document.getElementsByClassName("small-screen-hide");
		for ( let i = 0; i < itemsToShowOnBigScreen.length; i++ ) {
			itemsToShowOnBigScreen[i].style.display = "block";
		}
	},
	
	toggleNavbar: function() {
		if ( ui.navbarIsBig ) {
			ui.shrinkNavbar();
			ui.navbarIsBig = false;
		} else {
			ui.growNavbar();
			ui.navbarIsBig = true;
		}
	},

	/** @brief switch to a different page tab */
	openPage: function(pageName, elmnt, color)
	{
		// hide all tabs
	    var i, tabdiv, tablinks;
	    tabdiv = document.getElementsByClassName("tabdiv");
	    for (i = 0; i < tabdiv.length; i++) {
	        tabdiv[i].style.display = "none";
	    }

	    // un-highlight all tabs
	    tablinks = document.getElementsByClassName("tablink");
	    for (i = 0; i < tablinks.length; i++) {
	        tablinks[i].style.backgroundColor = "";
	    }

	    // show selected tab
	    document.getElementById(pageName).style.display = "flex";
	    elmnt.style.backgroundColor = color;

	    // Right menu has nothing in it for spot values, so hide it
	    var mainRights = document.getElementsByClassName("main-right");
	    if ( pageName == "spotvalues" ) {
			for ( i = 0; i < mainRights.length; i++ ) {
				mainRights[i].style.display = "none";
			}
	    } else {
			for ( i = 0; i < mainRights.length; i++ ) {
				mainRights[i].style.display = "block";
			}
	    }
	},

	/** @brief excutes when page finished loading. Creates tables and chart */
	onLoad: function()
	{
		// Set up listener to execute commands when enter is pressed (dashboard, command box)
		var commandinput = document.getElementById('commandinput');
		commandinput.addEventListener("keyup", function(event)
		{
			if ( event.keyCode == 13 )
			{
	            event.preventDefault();
	            ui.dashboardCommand();
			}
		});

		ui.updateTables();
		plot.generateChart();
		wifi.populateWiFiTab();
		ui.populateFileList();
		ui.refreshStatusBox();
		ui.refreshMessagesBox();
		ui.setAutoReload(true);
	},

	/** @brief automatically update data on the UI */
	refresh: function()
	{
		ui.updateTables();
		ui.refreshStatusBox();
		ui.refreshMessagesBox();
	},

	/** @brief send arbitrary command to inverter and print result
	 * @param cmd command string to be sent */
	sendCmd: function(cmd)
	{
		inverter.sendCmd(cmd, function(reply)
		{
			document.getElementById("message").innerHTML = reply;
		});
	},

	/** @brief generates parameter and spotvalue tables */
	updateTables: function()
	{
		var tableParam = document.getElementById("params");

		// Don't run if any one of the param boxes are highlighted (i.e. don't clobber what the user is typing)
		var paramFields = tableParam.querySelectorAll('input, select');
		for ( var i = 0; i < paramFields.length; i++ )
		{
			if ( paramFields[i] === document.activeElement )
			{
				return;
			}
		}

		document.getElementById("spinner-div").style.visibility = "visible";

		inverter.getParamList(function(values)
		{

			var tableSpot = document.getElementById("spotValues");
			var lastCategory = "";
			var params = {};

			while (tableParam.rows.length > 1) tableParam.deleteRow(1);
			while (tableSpot.rows.length > 1) tableSpot.deleteRow(1);

			for (var name in values)
			{
				var param = values[name];

				// Get docstring
				var docstring = docstrings.get(name);
				if ( ! docstring == "" )
				{
					var nameWithTooltip = "<div class=\"tooltip\">" + name + "<span class=\"tooltiptext\">" + docstring + "</span></div>";
				}
				else
				{
					nameWithTooltip = name;
				}

				if (param.isparam)
				{
					var valInput;
					var unit = param.unit;
					var index = "-";
					params[name] = param.value;

					// Initialise categoryVisible toggles if needed (on first load for example). Make visible by default.
					if ( !(param.category in ui.categoryVisible) )
						ui.categoryVisible[param.category] = true;

                    // If we're starting a new category, insert the category header row.
					if (param.category != lastCategory)
					{
						var icon = ui.categoryVisible[param.category] ? '-' : '+';
						ui.addRow(tableParam, [ '<BUTTON onclick="ui.toggleVisibility(\'' +
							param.category + '\');" style="background: none; border: none; font-weight: bold;">' + icon + ' ' +
							param.category + '</BUTTON>' ], true);
						// Colour the header row by category
						var hdr = tableParam.rows[tableParam.rows.length - 1];
						var cc = ui.getCategoryColor(param.category);
						hdr.style.borderLeft = '4px solid ' + cc;
						hdr.style.backgroundColor = ui.getCategoryBg(param.category);
						hdr.cells[0].style.color = cc;
						lastCategory = param.category;
					}

					if (param.enums)
					{
						if (param.enums[param.value])
						{

						    valInput = '<SELECT data-name="' + name + '" data-original="' + param.value + '" onchange="ui.markParamChanged(this)">';

						    for (var idx in param.enums)
						    {
	     						valInput += '<OPTION value="' + idx + '"';
							    if (idx == param.value)
								    valInput += " selected";
							    valInput += '>' + param.enums[idx] + '</OPTION>';
						    }
						}
						else
						{
	 						valInput = "<ul>";
	 						for (var key in param.enums)
	 						{
	 							if (param.value & key)
	 								valInput += "<li>" + param.enums[key];
	 						}
	 						valInput += "</ul>";
						}
						unit = "";
					}
					else
					{
						valInput = '<INPUT type="number" min="' + param.minimum + '" max="' + param.maximum +
							'" step="0.05" value="' + param.value + '" data-original="' + param.value +
							'" data-name="' + name + '" data-min="' + param.minimum + '" data-max="' + param.maximum +
							'" oninput="ui.markParamChanged(this)"/>';
					}

					if (param.i !== undefined)
					    index = param.i;

					ui.addRow(tableParam, [ index, nameWithTooltip, valInput, unit, param.minimum, param.maximum, param.default ], ui.categoryVisible[param.category]);
						// Colour the param row left border by category
						tableParam.rows[tableParam.rows.length - 1].style.borderLeft = '3px solid ' + ui.getCategoryColor(param.category);
				}
				else
				{
					var checkHtml = '<INPUT type="checkbox" data-name="' + name + '" data-axis="left" /> l';
					checkHtml += ' <INPUT type="checkbox" data-name="' + name + '" data-axis="right" /> r';
					var unit = param.unit;

					if (param.enums)
					{
						if (param.enums[param.value])
	 					{
	 						display = param.enums[param.value];
	 					}
	 					else
	 					{
	 						var active = [];
	 						for (var key in param.enums)
	 						{
	 							if (param.value & key)
	 								active.push(param.enums[key]);
	 						}
	 						display = active.join('|');
	 					}
						unit = "";
					}
					else
					{
						display = param.value;
					}

					ui.addRow(tableSpot, [ nameWithTooltip, display, unit ], true);
					// Colour spot value row by unit type
					var sr = tableSpot.rows[tableSpot.rows.length - 1];
					var sc = ui.getSpotColor(param.unit);
					sr.style.borderLeft = '3px solid ' + sc;
					sr.cells[1].style.color = sc;
					sr.cells[1].style.fontWeight = '500';
				}
			}
      ui.populateVersion();
			document.getElementById("paramDownload").href = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(params, null, 2));
			document.getElementById("spinner-div").style.visibility = "hidden";
		});
	},

	/** @brief Adds row to a table
	 * If table has multiple columns and only one cell value is
	 * provided, the cell is spanned across entire table
	 * @param table DOM object of table
	 * @param content Array of strings with contents for each cell */
	addRow: function(table, content, visible)
	{
		var tr = table.insertRow(-1); //add row to end
		tr.style.display = visible ? "" : "none";
		var colSpan = table.rows[0].cells.length - content.length + 1;

		for (var i = 0; i < content.length; i++)
		{
			var cell = tr.insertCell(-1);
			cell.colSpan = colSpan;
			cell.innerHTML = content[i];
		}
	},

	/** @brief fill out version box in the bottom left corner of the screen */
	populateVersion: function()
	{
		var versionDiv = document.getElementById("version");
		versionDiv.innerHTML = "";
		var firmwareVersion = String(paramsCache.get('version'));
		versionDiv.innerHTML += "firmware : " + firmwareVersion + "<br>";
		versionDiv.innerHTML += "web : v2.5"
	},

	/** @brief If beta features are visible, hide them. If hidden, show them. */
	toggleBetaFeaturesVisibility: function() {
		var betaFeatures = document.getElementsByClassName('beta-feature');
        var betaFeaturesCheckbox = document.getElementById('beta-features-checkbox');

		for ( var i = 0; i < betaFeatures.length; i++ )
		{
			if ( betaFeaturesCheckbox.checked )
			{
        betaFeatures[i].style.display = 'block';
			}
			else
			{
				betaFeatures[i].style.display = 'none';
			}
		}
	},

	/** @brief If beta features are visible, hide them. If hidden, show them. */
	setAutoReload: function(enable)	{
      var autoReloadCheckbox = document.getElementById('auto-reload-checkbox');

      // run the poll function every 2 seconds
      if (enable) {
        autoReloadCheckbox.checked = true;
        ui.autoRefreshHandle = setInterval(ui.refresh, 3000);
      }
      else {
        autoReloadCheckbox.checked = false;
        clearInterval(ui.autoRefreshHandle);
      }
	},

	/** @brief Show notification bar */
	showCommunicationErrorBar: function() {
		document.getElementById('communication-error-bar').style.display = 'block';
	},

	/** @brief Hide notification bar */
	hideCommunicationErrorBar: function() {
		document.getElementById('communication-error-bar').style.display = 'none';
	},

	/**
	 * ~~~ DASHBOARD ~~~
	 */

    /** @brief refresh the data in the status box (top left corner of dashboard page) */
	refreshStatusBox: function()
	{

		var statusDiv = document.getElementById('top-left');

		var status = paramsCache.get('status');

		if ( status == null ){
			return;
		}

		var lasterr = paramsCache.get('lasterr');
		var udc = paramsCache.get('udc');
		var tmphs = paramsCache.get('tmphs');
		var opmode = paramsCache.get('opmode');

		statusDiv.innerHTML = "";

		var tbl = document.createElement('table');
		var tbody = document.createElement('tbody');
		// status
		var tr = document.createElement('tr');
		var td = document.createElement('td');
		td.appendChild(document.createTextNode('Status'));
		tr.appendChild(td);
		td = document.createElement('td');
		td.appendChild(document.createTextNode(status));
		tr.appendChild(td);
		tbody.appendChild(tr);
		// opmode
		tr = document.createElement('tr');
	    td = document.createElement('td');
		td.appendChild(document.createTextNode('Opmode'));
		tr.appendChild(td);
		td = document.createElement('td');
		td.appendChild(document.createTextNode(opmode));
		tr.appendChild(td);
		tbody.appendChild(tr);
		// lasterr
		tr = document.createElement('tr');
		td = document.createElement('td');
		td.appendChild(document.createTextNode('Last error'));
		tr.appendChild(td);
		td = document.createElement('td');
		td.appendChild(document.createTextNode(lasterr));
		tr.appendChild(td);
		tbody.appendChild(tr);
		// udc
		tr = document.createElement('tr');
		td = document.createElement('td');
		td.appendChild(document.createTextNode('Battery voltage (udc)'));
		tr.appendChild(td);
		td = document.createElement('td');
		td.appendChild(document.createTextNode(udc));
		tr.appendChild(td);
		tbody.appendChild(tr);
		// tmphs
		tr = document.createElement('tr');
		td = document.createElement('td');
		td.appendChild(document.createTextNode('Inverter temperature'));
		tr.appendChild(td);
		td = document.createElement('td');
		td.appendChild(document.createTextNode(tmphs));
		tr.appendChild(td);
		tbody.appendChild(tr);


		tbl.appendChild(tbody);
		statusDiv.appendChild(tbl);

    },

    /** @brief execute command entered in the command box on the dashboard */
    dashboardCommand: function()
    {
    	// Get command entered
    	var commandinput = document.getElementById('commandinput').value;
    	// Get output box
    	var commandoutput = document.getElementById('commandoutput');
    	inverter.sendCmd(commandinput, function(reply){
            commandoutput.innerHTML += reply + "<br>";
            // Scroll output if needed
    	    commandoutput.scrollTop = commandoutput.scrollHeight;
    	});
    },

    /** @brief get error messages from inverter and put them in the messages box on the dashboard page */
    refreshMessagesBox: function(){
    	var messageBox = document.getElementById('message');
    	inverter.sendCmd('errors', function(reply){
            messageBox.innerHTML = reply;
    	});
    },


    /**
     * ~~~ UPDATE ~~~
     */

    /** @brief uploads file to web server, if bin-file uploaded, starts a firmware upgrade */
	uploadFile: function()
	{
		var xmlhttp = new XMLHttpRequest();
		var form = document.getElementById('uploadform');

		if (form.getFormData)
			var fd = form.getFormData();
		else
			var fd = new FormData(form);
		var file = document.getElementById('updatefile').files[0].name;

		xmlhttp.onload = function()
		{
			// Show popup reporting upload completion
			modal.emptyModal('small');
			modal.appendToModal('small', 'File upload complete');
			modal.showModal('small');
			// Refresh the list of files on the 'files' page
			ui.populateFileList();
			setTimeout(function() { modal.hideModal('small') }, 2000);
		}

		xmlhttp.open("POST", "/edit");
		xmlhttp.send(fd);
	},

    /**
     * ~~~ PARAMETERS ~~~
     */

    /** @brief Show modal box with the result of parameter update */
    showParamUpdateModal: async function(param, value)
    {
    	var c = 'set ' + param + ' ' + value;
    	modal.emptyModal('small');
    	modal.showModal('small');
    	modal.appendToModal('small', 'Setting ' + param + ' to ' + value + "<br>");
    	inverter.sendCmd(c, function(reply)
		{
			modal.appendToModal('small', reply);
		});
		await sleep(2000);
		modal.hideModal('small');
    },

    /** @brief Show confirmation that params have been saved */
    showParamsSavedModal: async function()
    {
    	ui.sendCmd('save');
    	modal.emptyModal('small');
    	var msg = "<p style=\"padding:20px;text-align:center;\">Parameters saved</p>";
    	modal.appendToModal('small', msg);
    	modal.showModal('small');
    	await sleep(2000);
    	modal.hideModal('small');
    },

    /** @brief Show a modal box asking user to confirm if they wish to restore params to those saved in flash */
	showRestoreParamsFromFlashConfirmationModal: function()
	{
		modal.emptyModal('small');
		var msg = "<p>Are you sure you want to discard any unsaved parameter settings and revert to the last saved state?";
		msg += "<div style=\"display:flex;\">";
		msg += "<button onclick=\"ui.restoreParamsFromFlash();\"><img class=\"buttonimg\" src=\"/icon-rotate-ccw.png\">Restore</button>";
		msg += "<button onclick=\"modal.hideModal('small');\"><img class=\"buttonimg\" src=\"/icon-x-square.png\">Cancel</button>";
		msg += "</div>";
		modal.appendToModal('small', msg);
		modal.showModal('small');
	},

	/** @brief Roll back any changes made to params to last saved state */
	restoreParamsFromFlash: function()
	{
		modal.hideModal('small');
		ui.sendCmd('load');
		ui.refresh();
	},

    /**
	 * ~~~ SPOT VALUES ~~~
	 */


	/**
	 * ~~~ PLOT & GAUGE ~~~
	 */


    /** @brief Add new field chooser to plot configuration form */
	addPlotItem: function()
	{
		// Get the form
		var plotFields = document.getElementById("plotConfiguration");

		// container for the two drop downs
		var selectDiv = document.createElement("div");
		selectDiv.classList.add('plotField');
		plotFields.appendChild(selectDiv);

		// Create a drop down and populate it with the possible spot values
		var selectSpotValue = document.createElement("select");
		selectSpotValue.classList.add('plotFieldSelect');
		for ( var key in paramsCache.getData() )
		{
			if ( ! paramsCache.getEntry(key).isparam )
			{
				var option = document.createElement("option");
				option.value = key;
				option.text = key;
				selectSpotValue.appendChild(option);
			}
		}
		selectDiv.appendChild(selectSpotValue);

		// Create the left/right drop down
		var selectLeftRight = document.createElement("select");
		selectLeftRight.classList.add("leftright");

		var optionLeft = document.createElement("option");
		optionLeft.value = 'left';
		optionLeft.text = 'left';
		selectLeftRight.appendChild(optionLeft);

		var optionRight = document.createElement("option");
		optionRight.value = 'right';
		optionRight.text = 'right';
		selectLeftRight.appendChild(optionRight);
		selectDiv.appendChild(selectLeftRight);

		// Add the delete button
		var deleteButton = document.createElement("button");
		var deleteButtonImg = document.createElement('img');
		deleteButtonImg.src = '/icon-trash.png';
		deleteButton.appendChild(deleteButtonImg);
		deleteButton.onclick = function() { this.parentNode.remove(); };
		selectDiv.appendChild(deleteButton);
	},

    /** @brief get the current configuration of the plot. I.e., what values should it show. */
	getPlotItems: function()
	{
		var items = {};
    	items.names = new Array();
	    items.axes = new Array();
		var formItems = document.forms["plotConfiguration"].elements;
		for ( var i = 0; i < formItems.length; i++ )
		{
            // Gather up field selections
			if ( formItems[i].type === 'select-one' && formItems[i].classList.contains('plotFieldSelect') )
			{
				items.names.push(formItems[i].value);
			}

			// Gather up left/right selections
			if ( formItems[i].type === 'select-one' && formItems[i].classList.contains('leftright') )
			{
				items.axes.push(formItems[i].value);
			}
		}
        return items;
	},

	/**
	 * DATA LOGGER
	 */

	/**
	 * FILES
	 */

    /** @brief populate the list of files table */
	populateFileList: function()
	{
		var filesTable = document.getElementById('filesTable');
		// emtpy the table
		while (filesTable.rows.length > 1) filesTable.deleteRow(1);
		// fetch file list and populate table
		inverter.getFiles(function(files)
		{
			for ( var i = 0; i < files.length; i++ )
			{
				var tr = filesTable.insertRow(-1);
				// filename name
				var fileNameCell = tr.insertCell(-1);
				fileNameCell.innerHTML = "<a href=" + files[i]['name'] + ">" + files[i]['name'] + "</a>";
				// delete button
				var deleteFileCell = tr.insertCell(-1);
				deleteFileCell.innerHTML = "<button onclick=\"ui.showDeleteFileConfirmationModal('" + files[i]['name'] + "');\"><img class=\"buttonimg\" src=\"/icon-trash.png\">Delete File</button>";
			}
		});
	},

	showDeleteFileConfirmationModal: function(filename)
	{
		modal.emptyModal('small');
		var msg = "<p>Are you sure you want to delete file '" + filename + "'?</p>";
		msg += "<div style=\"display:flex\">";
		msg += "<button onclick=\"ui.deleteFile('/" + filename + "');\"><img class=\"buttonimg\" src=\"/icon-trash.png\">Delete file</button>";
		msg += "<button onclick=\"modal.hideModal('small');\"><img class=\"buttonimg\" src=\"/icon-x-square.png\">Cancel</button>";
		msg += "</div>";
		modal.appendToModal('small', msg);
		modal.showModal('small');
	},

	deleteFile: function(filename)
	{
		var deleteFileRequest = new XMLHttpRequest();
		var params = {}
		params.f = "/" + filename;
		deleteFileRequest.onload = function()
    	{
    		// re-build file list
    		ui.populateFileList();
    		// hide modal
    		modal.hideModal('small');
    	};

    	deleteFileRequest.onerror = function()
		{
			alert("error");
		};

		deleteFileRequest.open("DELETE", "/edit?f=" + filename, true);
		deleteFileRequest.send();

	},

	/**
	 * WIFI SETTINGS
	 */

	/** @brief Returns a colour for a parameter category name (used for row colouring) */
	getCategoryColor: function(category) {
		var c = (category || '').toLowerCase();
		if (c.indexOf('motor')   >= 0 || c.indexOf('drive')   >= 0) return '#1565c0'; // blue
		if (c.indexOf('invert')  >= 0 || c.indexOf('power')   >= 0) return '#6a1b9a'; // purple
		if (c.indexOf('charg')   >= 0 || c.indexOf('batter')  >= 0) return '#2e7d32'; // green
		if (c.indexOf('throt')   >= 0 || c.indexOf('control') >= 0) return '#e65100'; // orange
		if (c.indexOf('temp')    >= 0 || c.indexOf('thermal') >= 0) return '#b71c1c'; // red
		if (c.indexOf('regen')   >= 0 || c.indexOf('brake')   >= 0) return '#4e342e'; // brown
		if (c.indexOf('can')     >= 0 || c.indexOf('comm')    >= 0) return '#00695c'; // teal
		if (c.indexOf('safety')  >= 0 || c.indexOf('limit')   >= 0) return '#c62828'; // dark red
		return '#37474f'; // default dark grey
	},

	/** @brief Returns a faint background tint matching getCategoryColor */
	getCategoryBg: function(category) {
		var c = (category || '').toLowerCase();
		if (c.indexOf('motor')   >= 0 || c.indexOf('drive')   >= 0) return '#e3f2fd';
		if (c.indexOf('invert')  >= 0 || c.indexOf('power')   >= 0) return '#f3e5f5';
		if (c.indexOf('charg')   >= 0 || c.indexOf('batter')  >= 0) return '#e8f5e9';
		if (c.indexOf('throt')   >= 0 || c.indexOf('control') >= 0) return '#fbe9e7';
		if (c.indexOf('temp')    >= 0 || c.indexOf('thermal') >= 0) return '#ffebee';
		if (c.indexOf('regen')   >= 0 || c.indexOf('brake')   >= 0) return '#efebe9';
		if (c.indexOf('can')     >= 0 || c.indexOf('comm')    >= 0) return '#e0f2f1';
		if (c.indexOf('safety')  >= 0 || c.indexOf('limit')   >= 0) return '#ffebee';
		return '#eceff1';
	},

	/** @brief Returns a colour for a spot value unit (used for row colouring) */
	getSpotColor: function(unit) {
		var u = (unit || '').toLowerCase().trim();
		if (u === 'a'   || u === 'ma')               return '#1565c0';
		if (u === 'v'   || u === 'mv')               return '#6a1b9a';
		if (u.indexOf('rpm') >= 0)                   return '#e65100';
		if (u.indexOf('\u00b0c') >= 0 || u === 'c')  return '#b71c1c';
		if (u === 'kw'  || u === 'w')                return '#2e7d32';
		if (u === '%')                               return '#00695c';
		return '#37474f';
	},

	// -------------------------------------------------------------------------
	// Parameter editing — staged apply with range validation and colour feedback
	// -------------------------------------------------------------------------

	/** @brief Mark an input/select as changed (orange) when the user edits it */
	markParamChanged: function(el) {
		var original = el.dataset.original;
		if (el.value !== original) {
			el.style.borderColor = '#ff9800';
			el.style.background  = '#fff8e1';
		} else {
			el.style.borderColor = '';
			el.style.background  = '';
		}
		// Show unsaved reminder
		var remind = document.getElementById('param-unsaved-remind');
		var hasChanges = document.querySelectorAll('#params input[style*="ff9800"], #params select[style*="ff9800"]').length > 0;
		if (remind) remind.style.display = hasChanges ? 'block' : 'none';
	},

	/** @brief Apply all changed parameter inputs — validate, send sequentially via SDO */
	applyParamChanges: function() {
		var inputs = document.querySelectorAll('#params input[oninput], #params select[onchange]');
		var toApply = [];
		var errors  = [];

		inputs.forEach(function(el) {
			if (el.style.borderColor !== 'rgb(255, 152, 0)' && el.style.borderColor !== '#ff9800') return;
			var name = el.dataset.name;
			var val  = el.value;
			var min  = parseFloat(el.dataset.min);
			var max  = parseFloat(el.dataset.max);

			// Range check for number inputs
			if (el.tagName === 'INPUT') {
				var v = parseFloat(val);
				if (isNaN(v)) {
					errors.push(name + ': not a number');
					el.style.borderColor = '#f44336';
					el.style.background  = '#fff0f0';
					return;
				}
				if (!isNaN(min) && !isNaN(max) && (v < min || v > max)) {
					errors.push(name + ': ' + v + ' out of range [' + min + ', ' + max + ']');
					el.style.borderColor = '#f44336';
					el.style.background  = '#fff0f0';
					return;
				}
			}
			toApply.push({ el: el, name: name, val: val });
		});

		if (errors.length) {
			document.getElementById('message').innerHTML =
				'<span style="color:#f44336">Error: ' + errors.join(' | ') + '</span>';
			return;
		}
		if (!toApply.length) {
			document.getElementById('message').innerHTML = 'No changes to apply.';
			return;
		}

		var btnApply = document.getElementById('btn-param-apply');
		if (btnApply) btnApply.disabled = true;
		document.getElementById('message').innerHTML = 'Sending ' + toApply.length + ' change(s)...';

		var idx = 0;
		function sendNext() {
			if (idx >= toApply.length) {
				if (btnApply) btnApply.disabled = false;
				document.getElementById('message').innerHTML =
					'<span style="color:#2e7d32">✓ ' + toApply.length + ' parameter(s) applied. Click Save to Flash to persist.</span>';
				var remind = document.getElementById('param-unsaved-remind');
				if (remind) remind.style.display = 'block';
				return;
			}
			var item = toApply[idx++];
			inverter.sendCmd('set ' + item.name + ' ' + item.val, function(reply) {
				if (reply && reply.trim() === '1') {
					item.el.style.borderColor = '#4CAF50';
					item.el.style.background  = '#e8f5e9';
					item.el.dataset.original  = item.val;
				} else {
					item.el.style.borderColor = '#f44336';
					item.el.style.background  = '#fff0f0';
					document.getElementById('message').innerHTML =
						'<span style="color:#f44336">Error on ' + item.name + ': ' + reply + '</span>';
				}
				setTimeout(sendNext, 150);
			});
		}
		sendNext();
	},

	/** @brief Save parameters to VCU flash — clears unsaved banner on success */
	saveParamChanges: function() {
		var btn = document.getElementById('btn-param-save');
		if (btn) btn.disabled = true;
		inverter.sendCmd('save', function(reply) {
			if (btn) btn.disabled = false;
			if (reply && reply.trim() === '1') {
				var remind = document.getElementById('param-unsaved-remind');
				if (remind) remind.style.display = 'none';
				// Reset all green inputs back to normal
				document.querySelectorAll('#params input[style], #params select[style]').forEach(function(el) {
					el.style.borderColor = '';
					el.style.background  = '';
				});
				document.getElementById('message').innerHTML =
					'<span style="color:#2e7d32">✓ Parameters saved to VCU flash.</span>';
			} else {
				document.getElementById('message').innerHTML =
					'<span style="color:#f44336">Save failed: ' + reply + '</span>';
			}
		});
	}
}

