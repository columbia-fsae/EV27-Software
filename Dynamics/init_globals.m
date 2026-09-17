function init_globals()
    global k0 C_x B_x E_x C_y B_y E_y D_x D_y
    global fz_x_points fz_y_points fx_d_list fy_d_list
    global eps_k

    load('TC_Model_Inputs.mat');

    eps_k = 1e-12;

    C_x = fx_fixed_params(1);
    B_x = fx_fixed_params(2);
    E_x = fx_fixed_params(3);

    C_y = fy_fixed_params(1);
    B_y = fy_fixed_params(2);
    E_y = fy_fixed_params(3);

end
