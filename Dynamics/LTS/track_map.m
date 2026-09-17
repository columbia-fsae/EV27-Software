function [xx, yy] = track_map(sim, channel)
    assert(all(size(channel) == size(sim.xx)));

    x = 0;
    y = 0;
    theta = 0;

    xx = zeros(size(sim.ii));
    yy = zeros(size(sim.ii));

    for j=1:length(sim.ii)
        i = sim.ii(j);
        if sim.stats.k(i) == 0
            x = x + sim.dx * cos(theta);
            y = y + sim.dx * sin(theta);
        else
            radius = -1/sim.stats.k(i);
            delta_theta = -sim.dx * sim.stats.k(i);

            cx = x - radius * sin(theta);
            cy = y + radius * cos(theta);

            x = cx + radius * sin(theta + delta_theta);
            y = cy - radius * cos(theta + delta_theta);

            theta = theta + delta_theta;
        end

        xx(j) = x;
        yy(j) = y;
    end

    hold on;

    %I = imread('2025endurance_rot.png');
    %h = image([-29,51.5],[-4.5,6.5],I);
    %uistack(h,'bottom')

    grid on;
    daspect([1 1 1])

    xticks(min(xx)-100:100:max(xx)+100);
    yticks(min(yy)-100:100:max(yy)+100);
    %fnplt(cscvn(xy), 'r', 2);

    % hold off;
    % axis equal;
    %
    % %figure(2)
    % hold on;
    % axis equal;

    % for k=1:size(test_x,2)
    %     color = [cmap(k) 0 (1 - cmap(k))];
    %     curr_list_x = test_x{k};
    %     curr_list_y = test_y{k};
    %     plot(curr_list_x,curr_list_y,'.','Color',color);
    % end
    channel = channel.';
    scatter(xx, yy, [], channel(sim.ii))
end
