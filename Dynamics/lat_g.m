%% Skidpad max lateral-g estimate via YMD (pure lateral, ax = 0)
init_globals();

v = 45/3.6;        % 45 kph -> m/s

% --- build solver -------------------------------------------------------
% YMDSolver(tw, w_bias, cgh, v, ax_target, FLLTD, cla, cp, toe_f, toe_r, b_bias)
tw     = 1.20;     % track width (m)            <-- set to your car
w_bias = 0.50;     % rear weight fraction        <-- set to your car
cgh    = 0.30;     % CG height (m)               <-- set to your car
FLLTD  = 0.50;     % front lateral LT fraction
cla    = 3.0;      % total lift coeff            <-- set to your car
cp     = 0.50;     % aero balance (frac front)
toe_f  = 0.0;  toe_r = 0.0;
b_bias = 0.60;     % unused when ax=0, constructor still needs it

solver = YMDSolver(tw, w_bias, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);

% --- sweep grid ---------------------------------------------------------
beta_list  = linspace(-8, 8, 41);    % sideslip (deg)
delta_list = linspace(-12, 12, 41);  % steer   (deg)

ay_trim_max = 0;   % max ay on the Mz = 0 (trimmed) contour
ay_env_max  = 0;   % raw envelope max (upper bound, not necessarily reachable)

for delta = delta_list
    ay = nan(size(beta_list));
    mz = nan(size(beta_list));
    for i = 1:numel(beta_list)
        [ay(i), mz(i)] = solver.solve_combined_slip(beta_list(i), delta);
    end
    ay_env_max = max(ay_env_max, max(abs(ay(isfinite(ay)))));

    % find Mz = 0 crossings along this delta isoline -> trimmed ay
    for i = 1:numel(beta_list)-1
        if isfinite(mz(i)) && isfinite(mz(i+1)) && mz(i)*mz(i+1) <= 0
            t        = mz(i) / (mz(i) - mz(i+1));      % linear interp to Mz=0
            ay_cross = ay(i) + t*(ay(i+1) - ay(i));
            ay_trim_max = max(ay_trim_max, abs(ay_cross));
        end
    end
end

fprintf('v = %.0f kph\n', v*3.6);
fprintf('Trimmed (Mz=0) max lateral accel : %.2f g\n', ay_trim_max);
fprintf('Envelope max (upper bound)       : %.2f g\n', ay_env_max);