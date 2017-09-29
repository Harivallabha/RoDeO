#include "rbf.hpp"
#include "Rodeo_macros.hpp"
#include "auxilliary_functions.hpp"

#include <armadillo>

using namespace arma;


double rbf_psi_linear(double r) {

    return r;
}
double rbf_psi_cubic(double r) {

    return r*r*r;
}
double rbf_psi_tps(double r) {

    return r*r*log(r);
}

double rbf_psi_gaussian(double r, double sigma) {

    return exp(-(r*r)/(2*sigma*sigma));
}


/** compute the coefficients of the Gram matrix for the Gaussian rbf
 *
 * @param[out] PSI
 * @param[in] X
 * @param[in] sigma
 */
void compute_PSI_gauss(mat &PSI, mat &X, double sigma, double lambda){

    int dim = PSI.n_rows;

#if 0
    printf("dimension the PSI matrix = %d\n",dim);
    PSI.print();
#endif



    for(unsigned i=0; i<dim; i++){ /* for each row of the PSI matrix other than the last row*/

        for(unsigned int j=0; j<dim; j++){

            rowvec x1 = X.row(i);
            rowvec x2 = X.row(j);

#if 0
            printf("x1 = \n");
            x1.print();
            printf("x2 = \n");
            x2.print();
#endif

            rowvec xdiff = x1 -x2;
            PSI(i,j) = rbf_psi_gaussian(L2norm(xdiff),sigma);

            /* add regularization term */
            if(i == j) {

                PSI(i,j)+= lambda;
            }


        }


    }

}

/** computes the surrogate model value
 *
 * @param[in] X  data matrix(normalized)
 * @param[in] xp point of evaluation(normalized)
 * @param[in] w rbf parameters (w0,w1,...,Wd)
 * @param[in] sigma : extra model parameters
 * @param[in] type
 * @return ftilde
 */
double calc_ftilde_rbf(mat &X, rowvec &xp, vec &w, int type, double sigma=1.0){

    unsigned int dim = X.n_rows;

#if 0
    printf("dim = %d\n",dim);
#endif

    double ftilde=0.0;

    for(int i=0; i<dim; i++){ /* loop through all basis functions */

        rowvec xi = X.row(i);
        rowvec xdiff = xp-xi;

        if(type == GAUSSIAN){

            ftilde += w(i)*rbf_psi_gaussian(L2norm(xdiff),sigma);
        }
        if(type == LINEAR){

            ftilde += w(i)*rbf_psi_linear(L2norm(xdiff));
        }

        if(type == CUBIC){

            ftilde += w(i)*rbf_psi_cubic(L2norm(xdiff));
        }

        if(type == THIN_PLATE_SPLINE){

            ftilde += w(i)*rbf_psi_tps(L2norm(xdiff));
        }

    }

    return ftilde;
}



/** compute the coefficients of the rbf model
 *
 * @param[in] X  data matrix(normalized)
 * @param[in] ys output values
 * @param[out] w rbf parameters (w0,w1,...,Wd)
 * @param[out] sigma
 * @param[in] lambda parameter of Tikhonov regularization
 * @param[in] type_of_basis_function
 * @return 0 if successful
 */

int train_rbf(mat &X, vec &ys, vec &w, double &sigma, int type){

    /* get the number of data points */
    unsigned int dim = X.n_rows;
    unsigned int d = X.n_cols;

    /* number of iterations for cross validation */
    unsigned int max_number_of_iter_for_cv = 100;
    unsigned int number_of_cv_inner_iter = 20;

    const double upper_bound_sigma = 5.0;
    const double lower_bound_sigma = 0.0;

    const double upper_bound_lambda = 6.0;
    const double lower_bound_lambda = 10.0;

    int number_of_data_points_for_cross_val = dim/5;

    int size_of_Gram_matrix = dim - number_of_data_points_for_cross_val;

#if 1
    printf("size of the Gram matrix = %d\n",size_of_Gram_matrix);
#endif



    /* Gram matrix */
    mat PSI (size_of_Gram_matrix,size_of_Gram_matrix );
    PSI.fill(0.0);

    /* rhs of the system PSI w = y */
    vec ysmod (size_of_Gram_matrix);

    /* array of indices used in cross validation */
    uvec cv_indices(number_of_data_points_for_cross_val);

    mat Xmod(size_of_Gram_matrix, d);

    vec wmod(size_of_Gram_matrix);
    wmod.fill(0.0);


    double min_squared_error = LARGE;
    double best_sigma = 0.0;
    double best_lambda = 0.0;
    /* outer iterations for the cross validation */
    for(int iter=0; iter<max_number_of_iter_for_cv; iter++){



        /* generate a random value for sigma and lambda*/
        double sigma = RandomDouble(lower_bound_sigma,upper_bound_sigma);
        double lambda = RandomDouble(lower_bound_lambda,upper_bound_lambda);
#if 0
        printf("sigma = %15.12f\n",sigma);

        lambda = pow(10.0, -lambda);
        printf("lambda = %15.12f\n",lambda);

#endif

        double squared_error = 0.0;

        /* inner iterations of the cross validation */
        for(int inner_iter =0; inner_iter<number_of_cv_inner_iter; inner_iter++){


#if 0
            printf("X = \n");
            X.print();
            printf("ys = \n");
            ys.print();
#endif

            /* generate a validation set */
            generate_validation_set(cv_indices, dim);
#if 0

            printf("cross validation indices = \n");
            cv_indices.print();
#endif
            /* Xmod = X-validation set; ysmod = ys-validation set */
            remove_validation_points_from_data(X, ys, cv_indices, Xmod, ysmod);

#if 0
            printf("Xmod = \n");
            Xmod.print();

            printf("ysmod = \n");
            ysmod.print();
#endif


            /* evaluate the Gram matrix */
            compute_PSI_gauss(PSI,Xmod,sigma,lambda);

#if 0
            printf("PSI matrix = \n");
            PSI.print();
#endif


            /* evaluate rbf weights */
            wmod = solve(PSI, ysmod);


#if 0
            printf("wmod = \n");
            wmod.print();
#endif


            double ftilde=0.0; /* surrogate model value */


            /* for all validation points */

            for(int point=0; point<number_of_data_points_for_cross_val; point++){

                rowvec xp = X.row((cv_indices(point)));
# if 0
                printf("point = %d\n",point);
                printf("cv_indices(point) = %d\n",cv_indices(point));
                printf("xp = \n");
                xp.print();
#endif

                calc_ftilde_rbf(Xmod,xp,wmod,type,sigma);

                double fexact = ys(cv_indices(point));
# if 0
                printf("ftilde = %10.7f\n",ftilde);
                printf("fexact = %10.7f\n",fexact);
#endif

                squared_error = pow((fexact-ftilde),2.0);

            }

        } /* inner cv iterations loop */


        squared_error = squared_error/number_of_cv_inner_iter;
#if 0

        printf("squared_error = %10.7f\n",squared_error);
#endif
        if (squared_error < min_squared_error){

            min_squared_error = squared_error;
            best_sigma  = sigma;
            best_lambda = lambda;
        }

    } /* outer cv iterations loop */





    /* allocate full Gram matrix */
    mat PSIfull(dim,dim);
    PSIfull.fill(0.0);

    best_lambda = pow(10.0, -best_lambda);

#if 1
    printf("best squared error = %10.7f\n",min_squared_error);
    printf("best lambda = %10.7f\n",best_lambda);
    printf("best sigma = %10.7f\n",best_sigma);
#endif

    /* evaluate the Gram matrix */
    compute_PSI_gauss(PSIfull,X,best_sigma,best_lambda);

#if 1
    printf("PSIfull = \n");
    PSIfull.print();
#endif


    /* evaluate rbf weights */
    w = solve(PSIfull, ys);

    sigma = best_sigma;

    return 0;
} 














