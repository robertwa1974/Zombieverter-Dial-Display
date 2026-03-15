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

var paramsCache = {

    data: undefined,
    dataById: {},
    failedFetchCount: 0,

    get: function(name) {
      if ( paramsCache.data !== undefined )
      {
        if ( name in paramsCache.data ) {
          if ( paramsCache.data[name].enums ) {
            if (paramsCache.data[name].enums[paramsCache.data[name].value])
			{
				return paramsCache.data[name].enums[paramsCache.data[name].value];
			}
			else
			{
				var active = [];
				for (var key in paramsCache.data[name].enums)
				{
					if (paramsCache.data[name].value & key)
						active.push(paramsCache.data[name].enums[key]);
				}
				return active.join('|');
			}
          } else {
            return paramsCache.data[name].value;
          }
        }
      }
      return null;
    },

    getEntry: function(name) {
      return paramsCache.data[name];
    },

    getData: function() { return paramsCache.data; },

    setData: function(data) {
      paramsCache.data = data;

      for (var key in data) {
        paramsCache.dataById[data[key].id] = data[key];
        paramsCache.dataById[data[key].id]['name'] = key;
      }
    },

    getJson: function() { return JSON.stringify(paramsCache.data); },

    getById: function(id) {
      return paramsCache.dataById[id];
    }
}

var inverter = {

	firmwareVersion: 0,

	/** @brief send a command to the inverter */
	sendCmd: function(cmd, replyFunc, repeat)
	{
		var xmlhttp=new XMLHttpRequest();
		var req = "/cmd?cmd=" + cmd;

		xmlhttp.onload = function()
		{
			if (replyFunc) replyFunc(this.responseText);
		}

		if (repeat)
			req += "&repeat=" + repeat;

		xmlhttp.open("GET", req, true);
		xmlhttp.send();
	},

	/** @brief fetch live spot values from /spot and merge into params */
	fetchSpotValues: function(params, replyFunc)
	{
		var spotReq = new XMLHttpRequest();
		spotReq.onload = function()
		{
			try {
				var spot = JSON.parse(this.responseText);
				for (var name in spot) {
					if (name in params) {
						params[name].value = spot[name];
					}
				}
			} catch(ex) {
				// ignore spot fetch errors — static values still show
			}
			paramsCache.setData(params);
			if (replyFunc) replyFunc(params);
		};
		spotReq.onerror = function()
		{
			// spot endpoint failed — still call replyFunc with static values
			paramsCache.setData(params);
			if (replyFunc) replyFunc(params);
		};
		spotReq.open("GET", "/spot", true);
		spotReq.send();
	},

	/** @brief get the params from the inverter */
	getParamList: function(replyFunc, includeHidden)
	{
		var cmd = includeHidden ? "json hidden" : "json";

		inverter.sendCmd(cmd, function(reply) {
			var params = {};
			try
			{
				params = JSON.parse(reply);

				for (var name in params)
				{
					var param = params[name];
					param.enums = inverter.parseEnum(param.unit);

					if (name == "version")
						inverter.firmwareVersion = parseFloat(param.value);
				}
				paramsCache.failedFetchCount = 0;
			}
			catch(ex)
			{
        paramsCache.failedFetchCount += 1;
        if ( paramsCache.failedFetchCount >= 2 ){
          ui.showCommunicationErrorBar();
        }
			}
			if ( paramsCache.failedFetchCount < 2 )
			{
				ui.hideCommunicationErrorBar();
			}
			// Fetch live spot values and merge before calling replyFunc
			inverter.fetchSpotValues(params, replyFunc);
		});
	},

	getValues: function(items, repeat, replyFunc)
	{
		var process = function(reply)
		{
			var expr = /(\-{0,1}[0-9]+\.[0-9]*)/mg;
			var signalIdx = 0;
			var values = {};

			for (var res = expr.exec(reply); res; res = expr.exec(reply))
			{
				var val = parseFloat(res[1]);

				if (!values[items[signalIdx]])
					values[items[signalIdx]] = new Array()
				values[items[signalIdx]].push(val);
				signalIdx = (signalIdx + 1) % items.length;
			}
			replyFunc(values);
		};

		if (inverter.firmwareVersion < 3.53 || items.length > 10)
			inverter.sendCmd("get " + items.join(','), process, repeat);
		else
			inverter.sendCmd("stream " + repeat + " " + items.join(','), process);
	},

	parseEnum: function(unit)
	{
		var expr = /(\-{0,1}[0-9]+)=([a-zA-Z0-9_\-\.]+)[,\s]{0,2}|([a-zA-Z0-9_\-\.]+)[,\s]{1,2}/g;
		var enums = new Array();
		var res = expr.exec(unit);

		if (res)
		{
			do
			{
				enums[res[1]] = res[2];
			} while (res = expr.exec(unit))
			return enums;
		}
		return false;
	},

	setParam: function(params, index)
	{
		var keys = Object.keys(params);

		if (index < keys.length)
		{
			var key = keys[index];
			modal.appendToModal('large', "Setting " + key + " to " + params[key] + "<br>");
			inverter.sendCmd("set " + key + " " + params[key], function(reply) {
				modal.appendToModal('large', reply + "<br>");
				modal.largeModalScrollToBottom();
				inverter.setParam(params, index + 1);
			});
		}
	},

	canMapping: function(direction, name, id, pos, bits, gain)
	{
		var cmd = "can " + direction + " " + name + " " + id + " " + pos + " " + bits + " " + gain;
		inverter.sendCmd(cmd);
	},

	getFiles: function(replyFunc)
	{
		var filesRequest = new XMLHttpRequest();
		filesRequest.onload = function()
		{
			var filesJson = JSON.parse(this.responseText);
			replyFunc(filesJson);
		}
		filesRequest.onerror = function()
		{
			alert("error");
		}
		filesRequest.open("GET", "/list", true);
		filesRequest.send();
	},

	deleteFile: function(filename, replyFunc)
	{
		var deleteFileRequest = new XMLHttpRequest();
		deleteFileRequest.onload = function()
		{
			var responseJson = JSON.parse(this.responseText);
			replyFunc(responseJson);
		}
		deleteFileRequest.onerror = function()
		{
			alert("error");
		}
		deleteFileRequest.open("DELETE", "/edit?f=" + filename, true);
		deleteFileRequest.send();
	}

};