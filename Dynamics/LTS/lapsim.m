%% Simulation Algorithm
function sim = lapsim(track, car)
    sim.track = track;
    sim.car = car;
    [sim.dx, sim.xx, sim.vv] = base_sim(sim.track, sim.car);
    [sim.dt, sim.tt, sim.splits] = laptime(sim.dx, sim.xx, sim.vv, sim.track);
    if isfield(track, "split")
        istart = floor(track.split.start / sim.dx);
        iend = floor(track.split.end / sim.dx);
        sim.ii = istart:iend;
        sim.time = sim.tt(iend) - sim.tt(istart);
    else
        sim.ii = 1:length(sim.xx);
        sim.time = sim.tt(end);
    end
    sim.stats = calculate_power(sim.track, sim.car, sim.ii, sim.xx, sim.vv, sim.dt, sim.tt, sim.time);
end

function [dx, xx, vv] = base_sim(track, car)
    fprintf("Simulating %s\n", track.description)
    N = length(track.segments.lengths);
    cumlength = cumsum(track.segments.lengths);
    total_length = cumlength(end);
    dx = track.max_dx; % max distance to step by
    n = ceil(total_length / dx);
    dx = total_length / n; % ensure even division
    xx = 0:dx:total_length;
    top_speed = min(car.speed_limit, (power_limit(track, car) / 0.5 / car.aero.cda / track.air_density)^(1/3));
    %fprintf("Finding speed limits\n")
    gross_limits = arrayfun(@(k) calculate_limit(track, car, k, top_speed), track.segments.curvature);
    %max_ays = gross_limits .^ 2 .* track.segments.curvature / 9.806
    gross_limits = min(gross_limits, track.segments.limits); % by segments
    fine_limits = arrayfun(@(x) gross_limits(find_segment(cumlength, x)), xx); % by dx steps
    vv = fine_limits;
    % for i = 1:N
    %     if speed_limits(prev(i, N)) < speed_limits(i)
    %         x0 = cumlengths(prev(i, N));
    %         smear = evolve(+dx, cumlength, track, x0, speed_limits(prev(i, N)), vv);
    %         vv = min(vv, smear);
    %     elseif speed_limits(prev(i, N)) > speed_limits(i)
    %         smear = evolve(-dx, cumlength, track, x0, speed_limits(i), vv);
    %         vv = min(vv, smear);
    %     end
    % end

    %fprintf("Evolving mechanics equations\n")
    for i = 1:n
        if fine_limits(prev(i, n)) < fine_limits(i)
            %fprintf("Accel %d / %d\n", i, n)
            x0 = xx(i);
            v0 = fine_limits(prev(i, n));
            smear = evolve(+dx, cumlength, track, car, i, x0, v0, vv);
            vv = min(vv, smear);
        elseif fine_limits(prev(i, n)) > fine_limits(i)
            %fprintf("Brake %d / %d\n", i, n)
            x0 = xx(prev(i, n));
            v0 = fine_limits(i);
            smear = evolve(-dx, cumlength, track, car, prev(i, n), x0, v0, vv);
            vv = min(vv, smear);
        end
    end
    % figure
    % plot(xx, fine_limits*3.6);
    % xlabel("Distance (m)")
    % ylabel("Velocity (km/h)")
    % hold on
    % plot(xx, vv * 3.6)
    % legend(["Max speed to complete sector" "Lap after time-evolution"])
end

function idx = find_segment(cumlength, x)
    idx = find(cumlength >= x, 1);
end

function p = prev(i, l)
    if i == 1
        p = l;
    else
        p = i - 1;
    end
end

function n = next(i, l)
    if i == l
        n = 1;
    else
        n = i + 1;
    end
end

function smear = evolve(dx, cumlength, track, car, i0, x0, v0, limits)
    smear = limits;
    i = i0;
    di = sign(dx);
    v = v0;
    x = x0;
    while v <= limits(i)
        segment_idx = find_segment(cumlength, x);
        k = track.segments.curvature(segment_idx);

        if sign(dx) == 1
            a = max_accel(track, car, k, v, limits(i));
        else
            a = max_brake(track, car, k, v, limits(i));
        end

        dv = a * dx / v;
        v = v + dv;

        smear(i) = v;

        if sign(dx) == 1
            i = next(i, length(limits));
        else
            i = prev(i, length(limits));
        end
    end
end

%% Mechanics
function limit = calculate_limit(track, car, k, top_speed)
    % turns out time-evolving is easier - just terminal velocity
    v = top_speed; dt = 0.1;
    if k ~= 0
        while true
            [long_error, lateral_error] = steady_state_limit(track, car, k, v, 0);
            if long_error < 0
                v = v + long_error / car.mass * dt;
            elseif lateral_error < 0
                v = v + lateral_error / car.mass * dt;
            elseif v > top_speed
                v = top_speed;
                break
            elseif long_error / car.mass < 0.01 * 9.806
                break
            else
                v = v + long_error / car.mass * dt * 0.01;
            end
        end
    end
    limit = v;
end

function P = power_limit(track, car)
    if track.power_limit
        P = min(car.base_power_limit, car.endur_power_limit);
    else
        P = car.base_power_limit;
    end
end

function [long_error, lateral_error] = steady_state_limit(track, car, k, v, a)
    downforce = 0.5 * track.air_density * car.aero.cla * v^2;
    drag = 0.5 * track.air_density * car.aero.cda * v^2;

    W = car.mass * 9.806;

    Fx = drag + a * car.mass;
    Fy = v^2 * k * car.mass;
    Fz = -W - downforce;
    Mx = -Fy * car.cg(3) - W * car.cg(2) - downforce * car.cp(2);
    My = Fx * car.cg(3) + W * car.cg(1) + downforce * car.cp(1);

    [fr, rr, rl, fl] = corner_weights(car, Mx, My, Fz);

    yf = car.tires.factor_y * (-fr * car.tires.model.muy(fr) + -fl * car.tires.model.muy(fl));
    yr = car.tires.factor_y * (-rr * car.tires.model.muy(rr) + -rl * car.tires.model.muy(rl));
    xr = car.tires.factor_x * (-rr * car.tires.model.mux() + -rl * car.tires.model.mux());

    lateral_error = (yf + yr) - abs(Fy);
    rear_lateral_necessary = max(0, abs(Fy) - yf);
    rear_long_available = sqrt(max(0, 1 - (rear_lateral_necessary / yr)^2));

    long_error = rear_long_available * xr - Fx;
end

function ay = max_lateral(track, car, v, initial_ay)
    k = initial_ay / v^2; gain = 0.8; long_gain = 2;
    while true
            [long_error, lateral_error] = steady_state_limit(track, car, k, v, 0);
            if long_error < 0
                k = k + long_error / car.mass / v^2 * long_gain;
            elseif lateral_error < 0
                k = k + lateral_error / car.mass / v^2 * gain;
            elseif min(lateral_error, long_error) / car.mass < 0.01 * 9.806
                break
            else
                k = k + min(lateral_error, long_error) / car.mass / v^2 * gain * 0.1;
            end
            if k < 0
                k = -k;
            end
    end
    ay = k * v^2;
end

function effective_mux = effective_mux(track, car, k, v, limit)
    mux = car.tires.model.mux() * car.tires.factor_x;
    if k == 0
        effective_mux = mux;
    else
        ay = k * v^2;
        max_ay = k * limit^2;
        effective_mux = sqrt(max(0, 1 - (ay/max_ay)^2)) * mux;
    end
end

function a = max_brake(track, car, k, v, limit)
    downforce = 0.5 * track.air_density * car.aero.cla * v^2;
    drag = 0.5 * track.air_density * car.aero.cda * v^2;

    mux = effective_mux(track, car, k, v, limit);
    %mux = car.tires.model.mux() * car.tires.factor_x;

    a = (-mux * (car.mass * 9.806 + downforce) - drag) / car.mass; % for now
end

function a = max_accel(track, car, k, v, limit)
    downforce = 0.5 * track.air_density * car.aero.cla * v^2;
    drag = 0.5 * track.air_density * car.aero.cda * v^2;

    power_limited_accel = power_limit(track, car) / car.mass / v;

    w = v / car.tires.radius * car.drive.ratio;
    max_torque = interp1(car.drive.motor.LUT_WM_W, car.drive.motor.LUT_WM_M, w);
    motor_limited_accel = max_torque * car.drive.count * car.drive.ratio * car.drive.efficiency / car.tires.radius / car.mass;

    mux = effective_mux(track, car, k, v, limit);

    W = car.mass * 9.806;
    Wr = W * car.cg(1) / car.l;
    rear_downforce = downforce * car.cp(1) / car.l;

    % ax = mux * (Wr + W h/l * ax/g + rear_downforce) / m
    % ax (1 - mux h/l W/mg) = mux Wr / m
    % ax = mux (Wr + rear_downforce) / m / (1 - mux h/l)
    traction_limited_accel = mux * (Wr + rear_downforce) / car.mass / (1 - mux * car.cg(3) / car.l);

    a = min([power_limited_accel, motor_limited_accel, traction_limited_accel]) - drag / car.mass;
end

function [fr, rr, rl, fl] = corner_weights(car, Mx, My, Fz)
    t = car.t;
    l = car.l;
    flt = car.flltd;
    rlt = 1 - car.flltd;

    M = [
        -t/2 -t/2 t/2 t/2; % Mx = t/2 * (left - right)
        0    -l   -l  0;   % My = - l * rear
        1    1    1   1;   % Fz = sum corners
        rlt -flt flt -rlt  % 0 = rlltd * lltf - flltd * lltr
    ];

    corners = M \ [Mx; My; Fz; 0];

    fr = corners(1);
    rr = corners(2);
    rl = corners(3);
    fl = corners(4);
end

%% Statistics
function [dt, tt, splits] = laptime(dx, xx, vv, track)
    cumlength = cumsum(track.segments.lengths);
    tt = 0 * xx;
    dt = 0 * xx;
    splits = 0 * track.segments.lengths;
    for i = 1:length(xx)
        dt(i) = dx / vv(i);
        tt(i) = tt(prev(i, length(xx))) + dt(i);
        segment_idx = find_segment(cumlength, xx(i));
        splits(segment_idx) = tt(i);
    end
end

function stats = calculate_power(track, car, ii, xx, vv, dt, tt, time)
    cumlength = cumsum(track.segments.lengths);
    stats.ptraction = xx * 0;
    stats.pbrakes = xx * 0;
    stats.pdrag = xx * 0;
    stats.pkinetic = xx * 0;
    stats.pelectric = xx * 0;
    stats.pmotor = xx * 0;
    stats.tmotor = xx * 0;
    stats.wmotor = vv / car.tires.radius * car.drive.ratio;
    stats.ax = xx * 0;
    stats.ay = xx * 0;
    stats.k = xx * 0;

    for i = 1:length(xx)
        stats.ax(i) = (vv(next(i, length(vv))) - vv(i)) / (dt(i));
        segment_idx = find_segment(cumlength, xx(i));
        stats.k(i) = track.segments.curvature(segment_idx);
        stats.ay(i) = vv(i)^2*stats.k(i);
        f = car.mass * stats.ax(i);
        stats.pkinetic(i) = -f * vv(i);
        drag = 0.5 * car.aero.cda * track.air_density * vv(i)^2;
        stats.pdrag(i) = -drag * vv(i);
        ft = f + drag;
        if ft > 0
            stats.ptraction(i) = ft * vv(i);
            stats.pmotor(i) = stats.ptraction(i) / car.drive.efficiency;
            stats.tmotor(i) = ft * car.tires.radius / car.drive.ratio / car.drive.efficiency;
            stats.numotor(i) = interp2(car.drive.motor.LUT_WME_W, car.drive.motor.LUT_WME_M, car.drive.motor.LUT_WME_E.', clip(stats.wmotor(i), car.drive.motor.LUT_WME_W(1), car.drive.motor.LUT_WME_W(end)), clip(stats.tmotor(i), car.drive.motor.LUT_WME_M(1), car.drive.motor.LUT_WME_M(end)), 'nearest');
            stats.pelectric(i) = stats.pmotor(i) / stats.numotor(i);
        else
            stats.pbrakes(i) = ft * vv(i);
        end
    end

    stats.avgmotor = sum((dt(ii) .* stats.pmotor(ii)))/time;
    stats.avgelectric = sum(dt(ii) .* stats.pelectric(ii))/time;
    stats.avgtraction = sum(dt(ii) .* stats.ptraction(ii))/time;
    stats.avgbrakes = sum(dt(ii) .* stats.pbrakes(ii))/time;
    stats.avgdrag = sum(dt(ii) .* stats.pdrag(ii))/time;
end
