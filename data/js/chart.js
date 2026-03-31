var ctx = document.getElementById('ProgramChart').getContext('2d');
var chart_update_id;
var chart;

var chartColors = {
  red: 'rgb(255, 99, 132)',
  blue: 'rgb(54, 162, 235)',
  yel: 'rgb(150, 100, 0)',
  green: 'rgba(0, 170, 70, 0.1)'
};

var config_with = {
  type: 'line',
  data: {
    datasets: [{
      label: 'Soll-Temperatur (Programm)',
      yAxisID: 'temperature',
      backgroundColor: 'transparent',
      borderColor: chartColors.blue,
      pointBackgroundColor: chartColors.blue,
      tension: 0.1,
      fill: false,
      data: [~CHART_DATA~]
    }, {
      label: 'Ist-Temperatur (aktuell)',
      yAxisID: 'temperature',
      backgroundColor: 'transparent',
      borderColor: chartColors.red,
      pointBackgroundColor: chartColors.red,
      tension: 0.1,
      fill: false,
      data: []                     // wird dynamisch gefüllt
    }]
  },
  options: {
    title: {
      display: true,
      text: '~PROGRAM_NAME~ — Soll vs. Ist Temperatur'
    },
    responsive: true,
    scales: {
      xAxes: [{
        type: 'time',
        time: {
          tooltipFormat: 'YYYY-MM-DD HH:mm',
          displayFormats: {
            millisecond: 'HH:mm:ss.SSS',
            second: 'HH:mm:ss',
            minute: 'HH:mm',
            hour: 'HH'
          }
        },
        scaleLabel: {
          display: true,
          labelString: 'Zeit'
        }
      }],
      yAxes: [{
        id: 'temperature',
        gridLines: { display: true },
        scaleLabel: {
          display: true,
          labelString: "Temperatur (°C)"
        }
      }]
    }
  }
};

var config_without = {
  type: 'line',
  data: {
    datasets: [{
      label: 'Loaded program: ~PROGRAM_NAME~',
      tension: 0.1,
      yAxisID: 'temperature',
      backgroundColor: 'transparent',
      borderColor: chartColors.blue,
      fill: false,
      data: [~CHART_DATA~]
    }]
  },
  options: {
    title: {
      display: true,
      text: 'Ready to run program temperature/time graph'
    },
    responsive: true,
    scales: {
      xAxes: [{
        type: 'time',
        time: {
          tooltipFormat: 'YYYY-MM-DD HH:mm',
          displayFormats: {
            millisecond: 'HH:mm:ss.SSS',
            second: 'HH:mm:ss',
            minute: 'HH:mm',
            hour: 'HH'
          }
        },
        scaleLabel: {
          display: true,
          labelString: 'Zeit'
        }
      }],
      yAxes: [{
        id: 'temperature',
        gridLines: { display: true },
        scaleLabel: {
          display: true,
          labelString: 'Temperatur (°C)'
        }
      }]
    }
  }
};

// Chart initialisieren
chart = new Chart(ctx, ~CONFIG~);

// 30 Sekunden Update – lädt current.csv und aktualisiert nur die Ist-Temperatur
function chart_update(){
  if(!chart) return;

  fetch('~LOG_FILE~')
    .then(response => response.text())
    .then(csvText => {
      const lines = csvText.trim().split('\n');
      const actualData = [];

      // Header überspringen und Daten parsen
      for(let i = 1; i < lines.length; i++){
        const cols = lines[i].split(',');
        if(cols.length >= 2){
          actualData.push({
            x: cols[0],           // time
            y: parseFloat(cols[1]) // actual_temp
          });
        }
      }

      // 2. Dataset (Ist-Temperatur) aktualisieren
      if(chart.data.datasets[1]){
        chart.data.datasets[1].data = actualData;
      }

      chart.update();
    })
    .catch(err => console.log("Chart update error:", err));

  chart_update_id = setTimeout(chart_update, 30000); // alle 30 Sekunden
}

// Starte das Update, wenn ein Programm läuft
if(~CONFIG~ === "config_with"){
  setTimeout(() => {
    chart_update();
  }, 2000); // erstmal kurz warten bis Chart initial geladen ist
}