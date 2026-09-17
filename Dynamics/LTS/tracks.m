%% Regulations
fsae_ev.power_limit = 80e3;
fsae_hybrid.power_limit = inf;

%% Tracks
icahn_loop.description = "Icahn Stadium Test Loop";
icahn_loop.location = "New York, NY";
icahn_loop.air_density = 1.293;
icahn_loop.segments.lengths   = [60 11.75*pi 20 15.25*pi/2 10 18.75*pi/2 40 11.75*pi/2 19 21.75*pi/2 14 3.25*pi/2 5 6.75*pi/2];
icahn_loop.segments.curvature = [0  -1/11.75 0  1/15.25    0  -1/18.75   0  -1/11.75   0  -1/21.75   0  1/3.25    0 -1/6.75];
icahn_loop.segments.limits    = Inf * ones(1, length(icahn_loop.segments.lengths));
icahn_loop.laps = 1;
icahn_loop.power_limit = false;
icahn_loop.max_dx = 0.1;

accel_event.description = "FSAE Acceleration Event";
accel_event.location = "Brooklyn, MI";
accel_event.air_density = 1.293;
accel_event.segments.lengths   = [10  0.3 75  100];
accel_event.segments.curvature = [0   0   0   0];
accel_event.segments.limits    = [0.1 Inf Inf Inf];
accel_event.laps = 1;
accel_event.split.start = 10 + 0.3;
accel_event.split.end = 10 + 75.3;
accel_event.power_limit = false;
accel_event.max_dx = 0.005;

skid_event.description = "FSAE Skidpad Event";
skid_event.location = "Brookyln, MI";
skid_event.air_density = 1.293;
R = 18.25/2; % min 15.25/2 + track/2, max 21.25/2 - track/2
skid_event.segments.lengths   = 2*pi*R;
skid_event.segments.curvature = 1/R;
skid_event.segments.limits    = Inf;
skid_event.laps = 1;
skid_event.power_limit = false;
skid_event.max_dx = 0.1;
% skid_event.segments.lengths   = [2*pi*R];
% skid_event.segments.curvature = [1/R];
% skid_event.segments.limits    = [Inf];

endur_event.description = "FSAE Endurance Event Michigan 2024";
endur_event.location = "Brooklyn, MI";
endur_event.air_density = 1.293;
endur_event.segments.lengths = [7 5*pi/6 2*pi/2.8 3 2*pi/3 4 1.5*pi/6 1.5 1*pi/6 1.4*pi/4 1.2*pi/3 0.7*pi/5 1.5 1.5*pi/3 1*pi/2.5 5*pi/10 5*pi/18 7 ...
1.5*pi/10 1*pi/2.2 1 1.5*pi/2.8 3 1*pi/1.2 0.9 1.5*pi/2.5 0.7 0.5*pi/1.8 5*pi/12 2 6.5*pi/3.5 2*pi/3.5 1.5 1*pi/3.5 3*pi/6.5 2 ...
0.7*pi/5 0.5*pi/4.6 5 3*pi/9 3*pi/5 1*pi/3.5 1*pi/2.5 1*pi/2.5 1*pi/2.5 1.5*pi/3.25 2.5 1*pi/2.3 1.7*pi/1.3 1.3*pi/2.6 3.5 0.5*pi/4 ...
1*pi/2.3 1*pi/2.5 1*pi/2.3 1 0.5*pi/2.4 1.3 0.5*pi/2.3 1.5 0.8*pi/2.1 1 0.8*pi/2.1 0.8 0.9*pi/1.4 2.1 1.5*pi/3 1.5 1*pi/2.7 1.7 ...
1.2*pi/4.5 2 1*pi/3.4 1.5454 2.4123];
endur_event.segments.curvature = [0 1/5 -1/2 0 1/2 0 -1/1.5 0 1/1 -1/1.2 1/1.2 -1/0.7 0 -1/1.5 1/1 -1/5 1/5 0 ...
-1/1.5 1/1 0 -1/1.5 0 -1/1 0 1/1.5 0 -1/0.5 1/5 0 -1/7 1/2 0 -1/1 1/3 0 ...
-1/0.7 1/0.5 0 1/3 -1/3 1/1 -1/1 1/1 -1/1 1/2 0 -1/1 1/1.7 -1/1.3 0 -1/0.5 ...
1/1 -1/1 1/1 0 -1/0.5 0 1/0.5 0 -1/0.8 0 1/0.8 0 -1/0.9 0 -1/1.5 0 -1/1 0 ...
1.2/1 0 -1/1 0 1/3];
factor = 1000 / sum(endur_event.segments.lengths);
endur_event.segments.lengths = factor * endur_event.segments.lengths;
endur_event.segments.curvature = 1/factor * endur_event.segments.curvature;
endur_event.segments.limits = Inf * ones(1, length(endur_event.segments.lengths));
endur_event.laps = 22;
endur_event.power_limit = true;
endur_event.max_dx = 0.1;

autox_event.description = "FSAE Autocross Event Michigan 2024";
autox_event.location = "Brooklyn, MI";
autox_event.air_density = 1.293;
autox_event.segments.lengths = [10 endur_event.segments.lengths];
autox_event.segments.curvature = [0 endur_event.segments.curvature];
autox_event.segments.limits = [0.1 endur_event.segments.limits];
autox_event.laps = 1;
autox_event.split.start = 10;
autox_event.split.end = 810;
autox_event.power_limit = false;
autox_event.max_dx = 0.1;