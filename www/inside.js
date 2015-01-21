$(document).ready(function() {
  window.setInterval(function() {
    if ($('#scoreboard').is(':visible')) {
      $('#scoreboard').hide();
      $('#video').show();
      $('body').removeClass("stripe")
    } else {
      $('#scoreboard').show();
      $('#video').hide();
      $('body').addClass("stripe")
    }
  }, 13 * 1000);
});

