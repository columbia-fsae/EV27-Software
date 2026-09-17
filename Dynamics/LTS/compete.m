function comp = compete(car)
    tracks
    power_tune_gain = 0.5;
    i = 0;
    while true
        accel = lapsim(accel_event, car);
        i = i + 1;
        max_power = max(accel.stats.pelectric(accel.ii));
        ff = car.base_power_limit * fsae_ev.power_limit / max_power;
        if max_power < fsae_ev.power_limit
            if fsae_ev.power_limit - max_power < 1.5e3
                break
            end
            if car.base_power_limit > fsae_ev.power_limit
                break
            end
            fb = car.base_power_limit + 1e3;
        else
            fb = car.base_power_limit - 5e3;
        end
        car.base_power_limit = power_tune_gain * ff + (1 - power_tune_gain) * fb;
    end
    fprintf("Acceleration: %d tries power limit: %d kW; max power: %d kW\n", i, car.base_power_limit/1e3, max_power/1e3)
    skid  = lapsim(skid_event, car);
    autox = lapsim(autox_event, car);
    energy_tune_gain = 0.5;
    i = 0;
    while true
        endur = lapsim(endur_event, car);
        i = i + 1;
        energy = endur.stats.avgelectric * endur.time * endur_event.laps;
        fprintf("Endurance: %d tries power limit: %d kW; total energy: %d kWh\n", i, car.endur_power_limit/1e3, energy/3.6e6)
        if energy < car.hv.energy
            if energy / car.hv.energy > 0.99
                break
            end
            if car.endur_power_limit > fsae_ev.power_limit
                break
            end
            fb = car.endur_power_limit;
        else
            fb = car.endur_power_limit - 5e3;
        end
        ff = car.endur_power_limit * car.hv.energy / energy;
        car.endur_power_limit = energy_tune_gain * ff + (1 - energy_tune_gain) * fb;
    end
    comp.car = car;
    comp.accel = accel;
    comp.skid = skid;
    comp.autox = autox;
    comp.endur = endur;

    comp.accel_time = accel.time;
    comp.skid_time = skid.time;
    comp.autox_time = autox.time;
    comp.endur_time = endur.time * endur_event.laps;
    [comp.score, comp.accel_score, comp.skid_score, comp.autox_score, comp.endur_score] = lap2score(comp.accel_time, comp.skid_time, comp.autox_time, comp.endur_time);
end