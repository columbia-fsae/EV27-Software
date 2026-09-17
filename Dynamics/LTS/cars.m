%% Motors
emrax208.max_rpm = 7000;
emrax228.max_rpm = 5000;
emrax268.max_rpm = 4500;
dhx_k40.max_rpm = 6000;

% speed-torque characteristics:
emrax208.LUT_WM_W = 0.10472*[0;4000;4500;5000;5500;6000;7000];
emrax208.LUT_WM_M = [140;135;132;128;122;115;0];

emrax228.LUT_WM_W = 0.10472*[0;2000;3000;4000;5000;5001];
emrax228.LUT_WM_M = [240;240;234;228;216;0];

emrax268.LUT_WM_W = 0.10472*[0;2000;4500;4501];
emrax268.LUT_WM_M = [500;500;450;0];

dhx_k40.LUT_WM_W = 0.10472*[0;6000;6001];
dhx_k40.LUT_WM_M = [80;80;0];

% efficiency maps
emrax208.LUT_WME_W = 0.10472*[500;1000;1500;2000;2500;3000;3500;4000;4500;5000];
emrax208.LUT_WME_M = [20;40;60;80;100;120;140];
emrax208.LUT_WME_E = [86 88 89 88 86 84 82;88 94 94 94 92 89 85;89 94.5 95.5 96 94.5 91 85;89 95.5 96 96 95 92 85;89 95.5 96 96 95 92 85;89 95 96 95.75 94.75 92 85;88 94.5 96 95.5 94.5 92 85;86 94 95 95.25 94.5 92 85;80 87 94 94.5 94.5 91 85;80 80 86 94 94 90 86]/100;

emrax228.LUT_WME_W = 0.10472*[500;1000;1500;2000;2500;3000;3500;4000;4500;5000]*5500/6000;
emrax228.LUT_WME_M = [20;40;60;80;100;120;140]*240/140;
emrax228.LUT_WME_E = [86 88 89 88 86 84 82;88 94 94 94 92 89 85;89 94.5 95.5 96 94.5 91 85;89 95.5 96 96 95 92 85;89 95.5 96 96 95 92 85;89 95 96 95.75 94.75 92 85;88 94.5 96 95.5 94.5 92 85;86 94 95 95.25 94.5 92 85;80 87 94 94.5 94.5 91 85;80 80 86 94 94 90 86]/100;

emrax268.LUT_WME_W = 0.10472*[500;1000;1500;2000;2500;3000;3500;4000;4500;5000]*4500/6000;
emrax268.LUT_WME_M = [20;40;60;80;100;120;140]*500/140;
emrax268.LUT_WME_E = [86 88 89 88 86 84 82;88 94 94 94 92 89 85;89 94.5 95.5 96 94.5 91 85;89 95.5 96 96 95 92 85;89 95.5 96 96 95 92 85;89 95 96 95.75 94.75 92 85;88 94.5 96 95.5 94.5 92 85;86 94 95 95.25 94.5 92 85;80 87 94 94.5 94.5 91 85;80 80 86 94 94 90 86]/100;

dhx_k40.LUT_WME_W = [0; 7000];
dhx_k40.LUT_WME_M = [0; 80];
dhx_k40.LUT_WME_E = [1 1; 1 1] * .93; % todo: fill in efficiency map

% hard power limit per motor
emrax208.P = 61.5e3;
emrax228.P = 80e3;
emrax268.P = 80e3;
dhx_k40.P = 40e3;

%% Tires
r20.model = load("R20_tire_model_updated.mat").tire_model;
r20.factor_y = 0.5;
r20.factor_x = 0.5;
r20.radius = 0.2032;

%% Cars
ev24.mass = 264 + 68;
ev24.cg = [0.738 0 0.25541];
ev24.aero.cda = 0.428;
ev24.aero.cla = 0;
ev24.drive.motor = emrax208;
ev24.drive.ratio = 3.7;
ev24.drive.efficiency = 0.96;
ev24.drive.count = 1;
ev24.tires.mux = 1.5; % TODO: get mu from chris
ev24.tires.muy = 1.5;
ev24.tires.rolling_resistance = 0.015;
ev24.tires.radius = 0.2032;
ev24.hv.vmax = 302.4;
ev24.hv.vnom = 260;

ev25.mass = 242.5 + 68;
ev25.cg = [0.738 0 0.25541];
ev25.aero.cda = 0.428;
ev25.aero.cla = 0;
ev25.drive.motor = emrax208;
ev25.drive.ratio = 4.9;
ev25.drive.efficiency = 0.96;
ev25.drive.count = 1;
ev25.tires = ev24.tires;
ev25.hv = ev24.hv;

% 2 x 268
ev26a.mass = 260 + 68;
ev26a.cg = ev25.cg;
ev26a.cp = ev26a.cg;
ev26a.t = 1.190;
ev26a.l = 1.530;
ev26a.flltd = 0.5;
ev26a.aero.cda = 0.2288;
ev26a.aero.cla = 0.51;
ev26a.drive.motor = emrax268;
ev26a.drive.ratio = 1;
ev26a.drive.efficiency = 0.96;
ev26a.drive.count = 2;
ev26a.tires = r20;
ev26a.hv.vmax = 600;
ev26a.hv.vnom = 520;
ev26a.hv.energy = 5.4 * 3.6e6; % J
ev26a.base_power_limit = 80e3;
ev26a.endur_power_limit = 40e3;
ev26a.speed_limit = 120 / 3.6;


% 1 x 208
ev26b.mass = 264;
ev26b.cg = ev25.cg;
ev26b.cp = ev26b.cg;
ev26b.t = 1.190;
ev26b.l = 1.530;
ev26b.flltd = 0.5;
ev26b.aero.cda = 0.2288; % will
ev26b.aero.cla = 3.1;
ev26b.drive.motor = emrax208;
ev26b.drive.ratio = 4.9;
ev26b.drive.efficiency = 0.96;
ev26b.drive.count = 1;
ev26b.tires = r20; % chris
ev26b.hv.vmax = 300;
ev26b.hv.vnom = 260;
ev26b.hv.energy = 5.4 * 3.6e6; % J
ev26b.base_power_limit = 80e3;
ev26b.endur_power_limit = 40e3;
ev26b.speed_limit = 120 / 3.6;
