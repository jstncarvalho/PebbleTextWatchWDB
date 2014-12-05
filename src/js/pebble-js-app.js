// Listen for when the watchface is opened
var locationOptions = { "timeout": 15000, "maximumAge": 60000 };

var xhrRequest = function (url, type, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
        callback(this.responseText);
    };
    xhr.open(type, url);
    xhr.send();
};

function locationSuccess(pos) {
    var url = "http://api.openweathermap.org/data/2.5/weather?lat=" + pos.coords.latitude + "&lon=" + pos.coords.longitude;
    xhrRequest(url, 'GET',
        function(responseText) {
            // responseText contains a JSON object with weather info
            var json = JSON.parse(responseText);

            // Temperature in Kelvin requires adjustment
            var temperature = Math.round(json.main.temp - 273.15);
            var low = Math.round(json.main.temp_min - 273.15);
            var high = Math.round(json.main.temp_max - 273.15);
            // Conditions
            var conditions = json.weather[0].main;
            conditions = conditions.toLowerCase();
            var dictionary = {
                "KEY_TEMPERATURE": temperature,
                "KEY_LOW": low,
                "KEY_HIGH": high,
                "KEY_CONDITIONS": conditions
            };

            Pebble.sendAppMessage(dictionary);
        }
    );
}

Pebble.addEventListener('appmessage',
    function(e) {
        getWeather();
    }
);

function locationError(err) {
    var dictionary = {
                "KEY_TEMPERATURE": 0,
                "KEY_LOW": 0,
                "KEY_HIGH": 0,
                "KEY_CONDITIONS": "X",
            };

            Pebble.sendAppMessage(dictionary);
}

function getWeather() {
    navigator.geolocation.getCurrentPosition(
        locationSuccess,
        locationError,
        locationOptions
    );
}

Pebble.addEventListener("ready",
    function(e) {
        console.log("PebbleKit JS ready!");
    }
);