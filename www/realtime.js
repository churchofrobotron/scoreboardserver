var sb_data = {"score_types" : [], "timestamp" : ""};

var realtime_sb = angular.module('realtime_sb', []);

realtime_sb.controller('sb_control', function($scope, $http, $timeout) {
  $scope.sb_data = sb_data;
  (function tick() {
    $http.get('leaderboard/').success(function(data) {
      if ($scope.sb_data.timestamp != data.timestamp)
        $scope.sb_data = data;
      $timeout(tick, 3000);
    }).error(function(data) {
      $timeout(tick, 3000);
    });
  })();
});
