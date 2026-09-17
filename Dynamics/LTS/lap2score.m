function [score, accel_score, skid_score, autox_score, endur_score] = lap2score(accel_time, skid_time, autox_time, endur_time)
    %Best times from 2025 Michigan
    accel_min = 3.821;
    skidpad_min = 4.933;
    autocross_min = 45.734;
    endurance_min = 1369.936;

    score = 25; %Assumes finishing all 22 laps in endurance

    %Accel
    accel_max = 1.5*accel_min;
    if accel_time < accel_min
        accel_score = 100;
    elseif accel_time < accel_max
        accel_score = 4.5+95.5*((accel_max/accel_time)-1)/((accel_max/accel_min)-1);
    else
        accel_score = 4.5;
    end

    %Skidpad
    skidpad_max = 1.25*skidpad_min;
    if skid_time < skidpad_min
        skid_score = 75;
    elseif skid_time < skidpad_max
        skid_score = 3.5 + 71.5*((skidpad_max/skid_time)^2-1)/((skidpad_max/skidpad_min)^2-1);
    else
        skid_score = 3.5;
    end

    %Autocross
    autocross_max = 1.45*autocross_min;
    if autox_time < autocross_min
        autox_score = 125;
    elseif autox_time < autocross_max
        autox_score = 6.5+118.5*((autocross_max/autox_time)-1)/((autocross_max/autocross_min)-1);
    else
        autox_score = 6.5;
    end

    %Endurance
    endurance_max = 1.45*endurance_min;
    if endur_time < endurance_min
        endur_score = 250;
    elseif endur_time < endurance_max
        endur_score = 250*((endurance_max/endur_time)-1)/((endurance_max/endurance_min)-1);
    end

    score = accel_score + skid_score + autox_score + endur_score;
end