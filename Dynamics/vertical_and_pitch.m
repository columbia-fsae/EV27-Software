%% Setup
close all; clear
m = 264; %
m_s = m - 14;
g = 9.81;   % gravitational accel
w = m*g;    % vehicle weight in N
frontbias = 0.5;         % percent of longitudinal weight distribution - front
rearbias  = 1-frontbias; % percent of longitudinal weight distribution - rear

h_RH = 0.04; %
% antidive = 0.14;

frontweight = w*frontbias; % weight on front axle
rearweight = w*rearbias;   % weight on rear axle

tw = 1.2; % track width in meters
tw_f = tw;
tw_r = tw;
wb = 1.530; % wheelbase in meters

cgh = 0.247; % Arbitrary target considering little change to ev24 for ev25

kt = 510;                % tire rate in lb/in, allegedly from hoosier (724)
convspring = 175.126835; % lb/in to n/m conversion
kt = kt*convspring;      % tire rate in N/m

% roll center heights in m
frontrollcenter = 0.06;
rearrollcenter = 0.065;
H = cgh - ((rearrollcenter-frontrollcenter)*rearbias + frontrollcenter);


%% New ride rates\
frontspring = 225 * convspring;
rearspring = 400 * convspring;
mr_f = 0.98;
mr_r = 0.69;
kw_f = frontspring * mr_f^2;
kw_r = rearspring * mr_r^2;
freq_f = 1/(2 * pi) * sqrt(kw_f/(m_s/4))
freq_r = 1/(2 * pi) * sqrt(kw_r/(m_s/4));

kr_f = (kw_f * kt)/(kw_f + kt);
kr_r = (kw_r * kt)/(kw_r + kt);

front_crit_damping = 2 * sqrt(kr_f * m/4)/1000;
rear_crit_damping = 2 * sqrt(kr_r * m/4)/1000;

f_damping_ratio = 0.4;
r_damping_ratio = 0.4;

f_damper_shaft_c = front_crit_damping * f_damping_ratio / mr_f;
r_damper_shaft_c = rear_crit_damping * r_damping_ratio / mr_r;

% fprintf("Front Damper Shaft Damping Const %d \n", f_damper_shaft_c);
% fprintf("Rear Damper Shaft Damping Const %d \n", r_damper_shaft_c);


kphi_f = kr_f * tw^2/2;
kphi_r = kr_r * tw^2/2;
kphi_sum = kphi_f + kphi_r;
rg = (w*H)/kphi_sum*180/pi;

%% Bottoming out under braking checks

% first figuring out max ax with aero
w_static_f = w * frontbias/2;
w_static_r = w * (1 - frontbias)/2;
SR_list = linspace(0, 0.4, 30);
v = 20;
ax_max = 0;
cp = 0.45; % maybe 20% shift forward (max?) according to william, ignore migration for now??
cla = 3.121;
cda = 1.7;
F_df_f = cla * v.^2 * cp * 1.225 * 1/2;
F_df_r = cla * v.^2 * (1 - cp) * 1.225 * 1/2;
F_drag = cda * v.^2 * 1.225 * 1/2;
for SR=SR_list
    ax_current = 1;
    diff = 1;
    while diff > 1e-3
        longLT = w * ax_current * cgh/wb;
        FZ = [w_static_f - longLT/2 + F_df_f/2, ...
              w_static_f - longLT/2 + F_df_f/2, ...
              w_static_r + longLT/2 + F_df_r/2, ...
              w_static_r + longLT/2 + F_df_r/2];
        fx_total = 0;
        for tire_idx = [3,4]
            [fx_tire_frame, ~, ~, ~, ~] = brushTireForce(SR, 0, -FZ(tire_idx));
            fx_total = fx_total + fx_tire_frame;
        end
        ax_new = fx_total/w;
        diff = abs(ax_new - ax_current);
        ax_current = ax_new;
    end

    if ax_current > ax_max
        ax_max = ax_current;
    end
end
ax_braking = ax_max;
fprintf('Max ax under accel with %f m/s: %f g \n', v, ax_braking)

cp_z = 0.604;
pch = 28.1 * 1e-3;
pc_x = 0.922; % where do u use this?

long_moment_elastic = ax_braking * w * (cgh) * 0.86 - F_drag * cp_z * 0.86;
long_LT = long_moment_elastic/wb;

braking_travel_f = (long_LT + F_df_f)/kr_f; % assuming dampers have 0 mass
braking_travel_r = (long_LT - F_df_r)/kr_r; % flip sign

spring_compression_f = braking_travel_f * mr_f;
spring_compression_tot = spring_compression_f + w/4/kr_f;

l_fw = 0.8782; % distance of frontmost fw to front axle
FBH_travel = braking_travel_f + l_fw * (braking_travel_f + braking_travel_r)/wb;
pitch_angle = rad2deg(atan((braking_travel_f + braking_travel_r)/wb));
pitch_gradient = pitch_angle/ax_braking; % assuming linearity
h_FBH = (h_RH - FBH_travel) * 1000; % height of FBH above ground in mm
fprintf('FW height above ground under full braking: %f mm \n', h_FBH)

%% Lateral accel
ay = 2;
wf = ay*(w/tw_f)*((H*kphi_f)/kphi_sum + rearbias*frontrollcenter);
wr = ay*(w/tw_r)*((H*kphi_r)/kphi_sum + frontbias*rearrollcenter);
FLLTD = wf/(wf + wr);
fprintf('Roll Gradient w/o ARB: %f deg/g \n', rg)
fprintf('FLLTD under 1.5 lat g w/o ARB: %f \n', FLLTD)

%% With existing ARB
kphi_farb = 0e4; % 6.8994e+04 designed value from old script
kphi_rarb = 5e4;
kphi_f_w_arb = (kt*tw^2*kphi_farb/2 + kw_f*kt*tw^4/4)/(kt*tw^2/2 + kphi_farb + kw_f*tw^2/2);
kphi_r_w_arb = (kt*tw^2*kphi_rarb/2 + kw_r*kt*tw^4/4)/(kt*tw^2/2 + kphi_rarb + kw_r*tw^2/2);
kphi_sum_w_arb = kphi_f_w_arb + kphi_r_w_arb;

wf_w_arb = ay*(w/tw_f)*((H*kphi_f_w_arb)/kphi_sum_w_arb + rearbias*frontrollcenter);
wr_w_arb = ay*(w/tw_r)*((H*kphi_r_w_arb)/kphi_sum_w_arb + frontbias*rearrollcenter);
FLLTD_w_arb = wf_w_arb/(wf_w_arb + wr_w_arb);
rg_w_arb = (w*H)/kphi_sum_w_arb*180/pi;
fprintf('Roll Gradient w ARB: %f deg/g \n', rg_w_arb)
fprintf('FLLTD under 1.5 lat g w ARB: %f \n', FLLTD_w_arb)
