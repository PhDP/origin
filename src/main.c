#define ORIGIN_DATE       "2012.01.05"
#define ORIGIN_VERSION    "2.1"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include "common.h"
#include "ivector.h"
#include "species.h"
#include "specieslist.h"
#include "graph.h"
#include "utils.h"

#define MODEL_BDM_NEUTRAL      0
#define MODEL_BDM_SELECTION    1

// Parameters of the simulations.
typedef struct
{
    int m;             // Type of model.
    int communities;   // Number of communities.
    int j_per_c;       // Number of individuals per community.
    int k_gen;         // Number of generations (in thousands).
    int init_species;  // Initial number of species.
    double mu;         // Mutation rate.
    double omega;      // Weight of the links between communities.
    double s;          // Selection coefficient.
    double r;          // Radius (for random geometric graphs).
    double w;          // Width (for rectangle random geometric graphs).
    char *ofilename;   // Name of the output files.
    char *shape;       // Shape of the metacommunity.
}
Params;

// Prototype for the function used by the threads
void *sim(void *parameters);
// The function used to setup the cumulative jagged array from the graph.
double **setup_cumulative_list(const graph *g, double omega);

/////////////////////////////////////////////////////////////
// Main                                                    //
/////////////////////////////////////////////////////////////
int main(int argc, const char *argv[])
{
    // Set default values;
    Params p;
    p.m = MODEL_BDM_SELECTION;
    p.communities = 10;
    p.j_per_c = 10000;
    p.init_species = 20;
    p.k_gen = 100;
    p.mu = 1e-4;
    p.omega = 5e-4;
    p.s = 0.15;
    p.r = 0.25;
    p.w = 0.25;
    p.ofilename = (char*)malloc(50);
    p.shape = (char*)malloc(20);

    // Number of simulations;
    int n_threads = 1;

    // Options;
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-')
    {
        // --help
        if (argv[1][2] == 'h')
        {
            printf("Usage: ./origin [options]\n");
            printf("Example: ./origin -x=8 -shape=circle -model=1 -s=0.10\n");
            printf("General options:\n");
            printf("  --help          How you got here...\n");
            printf("  --version       Display version.\n");
            printf("  --ref           Display reference.\n");
            printf("Simulation parameters:\n");
            printf("  -x\n");
            printf("    description:  Number of simulations to run. Each simulation\n");
            printf("                  will use a distinct POSIX thread. \n");
            printf("    values:       Any unsigned integer.\n");
            printf("    default:      1\n");
            printf("  -model\n");
            printf("    description:  Set the model used.\n");
            printf("    values:       0, 1.\n");
            printf("    details:      0 = Neutral BDM speciation.\n");
            printf("                  1 = BDM speciation with selection.\n");
            printf("    default:      1\n");
            printf("  -shape\n");
            printf("    description:  Set the shape of the metacommunity.\n");
            printf("    values:       circle, complete, random, rectangle, star.\n");
            printf("    default:      random\n");
            printf("  -r\n");
            printf("    description:  Threshold radius for random geometric\n");
            printf("                  graphs.\n");
            printf("    values:       Any double greater than 0.\n");
            printf("    default:      0.25\n");
            printf("  -w\n");
            printf("    description:  The width of a rectangle random geometric\n");
            printf("                  graph.\n");
            printf("    values:       0 < w < 1\n");
            printf("    default:      0.25\n");
            printf("  -g\n");
            printf("    description:  Number of generations in thousands.\n");
            printf("    values:       Any positive integer.\n");
            printf("    default:      100 (i.e.: 100 000 generations)\n");
            printf("  -c\n");
            printf("    description:  Number of local communities.\n");
            printf("    values:       Any integer greater than 0.\n");
            printf("    default:      10\n");
            printf("  -jpc\n");
            printf("    description:  Number of individuals per communities.\n");
            printf("    values:       Any integer greater than 0.\n");
            printf("    default:      10000\n");
            printf("  -sp\n");
            printf("    description:  Initial number of species. Must be a factor\n");
            printf("                  of the number of individuals/community.\n");
            printf("    values:       Any positive integer greater than 0.\n");
            printf("    default:      20\n");
            printf("  -s\n");
            printf("    description:  Set the selection coefficient (model 1 only).\n");
            printf("    values:       Any double in the (-1.0, 1.0) interval.\n");
            printf("    default:      0.15\n");
            printf("  -mu\n");
            printf("    description:  Set the mutation rate.\n");
            printf("    values:       Any positive double.\n");
            printf("    default:      1e-4\n");
            printf("  -omega\n");
            printf("    description:  The weight of the proper edges.\n");
            printf("    values:       Any nonnegative double.\n");
            printf("    default:      5e-4\n");
            printf("  -o\n");
            printf("    description:  Name of the output files.\n");
            printf("    values:       Any string.\n");
            return EXIT_SUCCESS;
        }
        else if (argv[1][2] == 'v') // --version
        { 
            printf("origin v.%s (%s)\n", ORIGIN_VERSION, ORIGIN_DATE);
            printf("Copyright (c) 2010-2011 Philippe Desjardins-Proulx <philippe.d.proulx@gmail.com>\n");
            printf("GPLv2 license  (see LICENSE)\n");
            return EXIT_SUCCESS;
        }
        else if (argv[1][2] == 'r') // --ref
        { 
            printf("P. Desjardins-Proul and D. Gravel. How likely is speciation in\n  neutral ecology? The American Naturalist 179(1).\n");
            return EXIT_SUCCESS;
        }
    } // end '--' options

    // Read options
    read_opt_i("g", argv, argc, &p.k_gen);
    read_opt_i("jpc", argv, argc, &p.j_per_c);
    read_opt_i("model", argv, argc, &p.m);
    read_opt_i("c", argv, argc, &p.communities);
    read_opt_i("sp", argv, argc, &p.init_species);
    read_opt_i("x", argv, argc, &n_threads);
    read_opt_d("mu", argv, argc, &p.mu);
    read_opt_d("omega", argv, argc, &p.omega);
    read_opt_d("r", argv, argc, &p.r);
    read_opt_d("w", argv, argc, &p.w);
    read_opt_d("s", argv, argc, &p.s);
    if (read_opt_s("o", argv, argc, p.ofilename) == false)
    {
        sprintf(p.ofilename, "");
    }
    if (read_opt_s("shape", argv, argc, p.shape) == false)
    {
        sprintf(p.shape, "random");
    }

    printf("<?xml version=\"1.0\"?>\n");
    printf("<origin_ssne>\n");
    printf("  <model>");
    if (p.m == MODEL_BDM_NEUTRAL)
    {
        printf("Neutral BDM speciation</model>\n");
    }
    else if (p.m == MODEL_BDM_SELECTION)
    {
        printf("BDM speciation with selection</model>\n");
    }
    printf("  <n>%d</n>\n", n_threads);
    printf("  <shape_metacom>%s</shape_metacom>\n", p.shape);
    printf("  <metacom_size>%d</metacom_size>\n", p.j_per_c * p.communities);
    printf("  <k_gen>%d</k_gen>\n", p.k_gen);
    printf("  <num_comm>%d</num_comm>\n", p.communities);
    printf("  <individuals_per_comm>%d</individuals_per_comm>\n", p.j_per_c);
    printf("  <initial_num_species>%d</initial_num_species>\n", p.init_species);
    printf("  <mutation_rate>%.2e</mutation_rate>\n", p.mu);
    printf("  <omega>%.2e</omega>\n", p.omega);
    if (p.m == MODEL_BDM_SELECTION)
    {
        printf("  <selection>%.2e</selection>\n", p.s);
    }
    if (p.shape[0] == 'r')
    {
        printf("  <radius>%.4f</radius>\n", p.r);
    }
    if (p.shape[0] == 'r' && p.shape[1] == 'e')
    {
        printf("  <width>%.4f</width>\n", p.w);
    }
    printf("  <filename>%s</filename>\n", p.ofilename);

    // The threads:
    pthread_t threads[n_threads];

    const time_t start = time(NULL);

    // Create the threads:
    for (int i = 0; i < n_threads; ++i)
    {
        pthread_create(&threads[i], NULL, sim, (void*)&p);
    }

    // Wait for the threads to end:
    for (int i = 0; i < n_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    const time_t end_t = time(NULL);
    printf("  <seconds>%lu</seconds>\n", (unsigned long)(end_t - start));
    printf("  <time>%s</time>\n", sec_to_string(end_t - start));
    printf("</origin_ssne>\n");
    return EXIT_SUCCESS; // yeppie !
}

///////////////////////////////////////////
// The simulation                        //
///////////////////////////////////////////
void *sim(void *parameters)
{
    const Params P = *((Params*)parameters);

    char *shape = P.shape;
    const int k_gen = P.k_gen;
    const int communities = P.communities;
    const int j_per_c = P.j_per_c;
    const int init_species = P.init_species;
    const int init_pop_size = j_per_c / init_species;
    const double omega = P.omega;
    const double mu = P.mu;
    const double s = P.s;
    const double radius = P.r;
    const double width = P.w;

    // GSL's Taus generator:
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_taus2);
    // Initialize the GSL generator with /dev/urandom:
    const unsigned int seed = devurandom_get_uint();
    gsl_rng_set(rng, seed); // Seed with time
    printf("  <seed>%u</seed>\n", seed);
    // Used to name the output file:
    char *buffer = (char*)malloc(100);
    // Nme of the file:
    sprintf(buffer, "%s%u.xml", P.ofilename, seed);
    // Open the output file:
    FILE *restrict out = fopen(buffer, "w");
    // Store the total num. of species/1000 generations:
    int *restrict total_species = (int*)malloc(k_gen * sizeof(int));
    // Number of speciation events/1000 generations:
    int *restrict speciation_events = (int*)malloc(k_gen * sizeof(int));
    // Number of extinctions/1000 generations:
    int *restrict extinction_events = (int*)malloc(k_gen * sizeof(int));
    // Number of speciation events/vertex:
    int *restrict speciation_per_c = (int*)malloc(communities * sizeof(int));
    // Number of local extinction events/vertex:
    int *restrict extinction_per_c = (int*)malloc(communities * sizeof(int));
    // Store the lifespan of the extinct species:
    ivector lifespan;
    ivector_init0(&lifespan);
    // Store the population size at speciation event:
    ivector pop_size;
    ivector_init0(&pop_size);
    // (x, y) coordinates for the spatial graph:
    double *restrict x = (double*)malloc(communities * sizeof(double));
    double *restrict y = (double*)malloc(communities * sizeof(double));
    for (int i = 0; i < communities; ++i)
    {
        speciation_per_c[i] = 0;
        extinction_per_c[i] = 0;
    }
    // Initialize an empty list of species:
    species_list *restrict list = species_list_init();
    // Initialize the metacommunity and fill them with the initial species evenly:
    for (int i = 0; i < init_species; ++i)
    {
        // Intialize the species and add it to the list:
        species_list_add(list, species_init(communities, 0, 3));
    }
    // To iterate the list;
    slnode *it = list->head;
    // Fill the communities:
    while (it != NULL)
    {
        for (int i = 0; i < communities; ++i)
        {
            it->sp->n[i] = init_pop_size;
            it->sp->genotypes[0][i] = init_pop_size;
        }
        it = it->next;
    }

    // To iterate the list;
    const int remainder = j_per_c - (init_species * init_pop_size);
    for (int i = 0; i < communities; ++i)
    {
        it = list->head;
        for (int j = 0; j < remainder; ++j, it = it->next)
        {
            ++(it->sp->n[i]);
            ++(it->sp->genotypes[0][i]);
        }
    }

    // Test (will be removed for v2.0):
    int sum = 0;
    it = list->head;
    while (it != NULL)
    {
        sum += species_total(it->sp);
        it = it->next;
    }
    assert(sum == j_per_c * communities);

    // Create the metacommunity;
    graph g;
    switch(shape[0])
    {
    case 's':
        shape = "star";
        graph_get_star(&g, communities);
        break;
    case 'c':
        if (shape[1] == 'o')
        {
            shape = "complete";
            graph_get_complete(&g, communities);
            break;
        }
        else
        {
            shape = "circle";
            graph_get_circle(&g, communities);
            break;
        }
    case 'r':
        if (shape[1] == 'e')
        {
            shape = "rectangle";
            graph_get_rec_crgg(&g, communities, width, radius, x, y, rng);
            break;
        }
        else
        {
            shape = "random";
            graph_get_crgg(&g, communities, radius, x, y, rng);
            break;
        }
    default:
        shape = "random";
        graph_get_crgg(&g, communities, radius, x, y, rng);
    }
    // Setup the cumulative jagged array for migration:
    double **cumul = setup_cumulative_list(&g, omega);

    fprintf(out, "<?xml version=\"1.0\"?>\n");
    fprintf(out, "<simulation>\n");
    fprintf(out, "  <model>");
    if (P.m == MODEL_BDM_NEUTRAL)
    {
        fprintf(out, "Neutral BDM speciation</model>\n");
    }
    else if (P.m == MODEL_BDM_SELECTION)
    {
        fprintf(out, "BDM speciation with selection</model>\n");
    }
    fprintf(out, "  <seed>%u</seed>\n", seed);
    fprintf(out, "  <shape_metacom>%s</shape_metacom>\n", shape);
    fprintf(out, "  <metacom_size>%d</metacom_size>\n", j_per_c * communities);
    fprintf(out, "  <k_gen>%d</k_gen>\n", k_gen);
    fprintf(out, "  <num_comm>%d</num_comm>\n", communities);
    fprintf(out, "  <individuals_per_comm>%d</individuals_per_comm>\n", j_per_c);
    fprintf(out, "  <initial_num_species>%d</initial_num_species>\n", init_species);
    fprintf(out, "  <mutation_rate>%.2e</mutation_rate>\n", mu);
    fprintf(out, "  <omega>%.2e</omega>\n", omega);
    if (P.m == MODEL_BDM_SELECTION)
    {
        fprintf(out, "  <selection>%.2e</selection>\n", s);
    }
    if (shape[0] == 'r')
    {
        fprintf(out, "  <radius>%.4f</radius>\n", radius);
    }
    if (shape[0] == 'r' && shape[1] == 'e')
    {
        fprintf(out, "  <width>%.4f</width>\n", width);
    }
    // To select the species and genotypes to pick and replace:
    slnode *s0 = list->head; // species0
    slnode *s1 = list->head; // species1
    int g0 = 0;
    int g1 = 0;
    int v1 = 0; // Vertex of the individual 1

    /////////////////////////////////////////////
    // Groups of 1 000 generations             //
    /////////////////////////////////////////////
    for (int k = 0; k < k_gen; ++k)
    {
        extinction_events[k] = 0;
        speciation_events[k] = 0;

        /////////////////////////////////////////////
        // 1 000 generations                       //
        /////////////////////////////////////////////
        for (int gen = 0; gen < 1000; ++gen)
        {
            const int current_date = (k * 1000) + gen;
            /////////////////////////////////////////////
            // A single generation                     //
            /////////////////////////////////////////////
            for (int t = 0; t < j_per_c; ++t)
            {
                /////////////////////////////////////////////
                // A single time step (for each community) //
                /////////////////////////////////////////////
                for (int c = 0; c < communities; ++c)
                {
                    // Select the species and genotype of the individual to be replaced
                    int position = (int)(gsl_rng_uniform(rng) * j_per_c);
                    s0 = list->head;
                    int index = s0->sp->n[c];
                    while (index <= position)
                    {
                        s0 = s0->next;
                        index += s0->sp->n[c];
                    }
                    position = (int)(gsl_rng_uniform(rng) * s0->sp->n[c]);
                    if (position < s0->sp->genotypes[0][c])
                    {
                        g0 = 0;
                    }
                    else if (position < (s0->sp->genotypes[0][c] + s0->sp->genotypes[1][c]))
                    {
                        g0 = 1;
                    }
                    else
                    {
                        g0 = 2;
                    }
                    // Choose the vertex for the individual
                    const double r_v1 = gsl_rng_uniform(rng);
                    v1 = 0;
                    while (r_v1 > cumul[c][v1])
                    {
                        ++v1;
                    }
                    v1 = g.adj_list[c][v1];
                    // species of the new individual
                    position = (int)(gsl_rng_uniform(rng) * j_per_c);
                    s1 = list->head;
                    index = s1->sp->n[v1];
                    while (index <= position)
                    {
                        s1 = s1->next;
                        index += s1->sp->n[v1];
                    }
                    if (v1 == c) // local remplacement
                    {
                        const double r = gsl_rng_uniform(rng);
                        const int aa = s1->sp->genotypes[0][v1];
                        const int Ab = s1->sp->genotypes[1][v1];
                        const int AB = s1->sp->genotypes[2][v1];

                        // The total fitness of the population 'W':
                        const double w = aa + Ab * (1.0 + s) + AB * (1.0 + s) * (1.0 + s);

                        if (r < aa / w)
                        {
                            g1 = gsl_rng_uniform(rng) < mu ? 1 : 0;
                        }
                        else
                        {
                            if (AB == 0 || r < (aa + Ab * (1.0 + s)) / w)
                            {
                                g1 = gsl_rng_uniform(rng) < mu ? 2 : 1;
                            }
                            else
                            {
                                g1 = 2;
                            }
                        }
                    }
                    else
                    { // Migration event
                        g1 = 0;
                    }
                    // Apply the changes
                    s0->sp->n[c]--;
                    s0->sp->genotypes[g0][c]--;
                    s1->sp->n[c]++;
                    s1->sp->genotypes[g1][c]++;

                    ////////////////////////////////////////////
                    // Check for local extinction             //
                    ////////////////////////////////////////////
                    if (s0->sp->n[c] == 0)
                    {
                        extinction_per_c[c]++;
                    }
                    ////////////////////////////////////////////
                    // Check for speciation                   //
                    ////////////////////////////////////////////
                    else if (s0->sp->genotypes[2][c] > 0 && s0->sp->genotypes[0][c] == 0 && s0->sp->genotypes[1][c] == 0)
                    {
                        species_list_add(list, species_init(communities, current_date, 3)); // Add the new species

                        const int pop = s0->sp->n[c];
                        list->tail->sp->n[c] = pop;
                        list->tail->sp->genotypes[0][c] = pop;
                        s0->sp->n[c] = 0;
                        s0->sp->genotypes[2][c] = 0;

                        // To keep info on patterns of speciation...
                        ivector_add(&pop_size, pop);
                        ++speciation_events[k];
                        ++speciation_per_c[c];
                    }

                } // End 'c'

            } // End 't'

            // Remove extinct species from the list and store the number of extinctions.
            extinction_events[k] += species_list_rmv_extinct2(list, &lifespan, current_date);

        } // End 'g'

        total_species[k] = list->size;

    } // End 'k'

    //////////////////////////////////////////////////
    // PRINT THE FINAL RESULTS                      //
    //////////////////////////////////////////////////
    fprintf(out, "  <global>\n");
    fprintf(out, "    <proper_edges>%d</proper_edges>\n", graph_edges(&g));
    fprintf(out, "    <links_per_c>%.4f</links_per_c>\n", (double)graph_edges(&g) / communities);

    fprintf(out, "    <avr_lifespan>%.4f</avr_lifespan>\n", imean(lifespan.array, lifespan.size));
    fprintf(out, "    <median_lifespan>%.4f</median_lifespan>\n", imedian(lifespan.array, lifespan.size));

    fprintf(out, "    <avr_pop_size_speciation>%.4f</avr_pop_size_speciation>\n", imean(pop_size.array, pop_size.size));
    fprintf(out, "    <median_pop_size_speciation>%.4f</median_pop_size_speciation>\n", imedian(pop_size.array, pop_size.size));

    fprintf(out, "    <speciation_per_k_gen>");
    int i = 0;
    for (; i < k_gen - 1; ++i)
    {
        fprintf(out, "%d ", speciation_events[i]);
    }
    fprintf(out, "%d</speciation_per_k_gen>\n", speciation_events[i]);

    fprintf(out, "    <extinctions_per_k_gen>");
    for (i = 0; i < k_gen - 1; ++i)
    {
        fprintf(out, "%d ", extinction_events[i]);
    }
    fprintf(out, "%d</extinctions_per_k_gen>\n", extinction_events[i]);

    fprintf(out, "    <extant_species_per_k_gen>");
    for (i = 0; i < k_gen - 1; ++i)
    {
        fprintf(out, "%d ", total_species[i]);
    }
    fprintf(out, "%d</extant_species_per_k_gen>\n", total_species[i]);

    // Print global distribution
    fprintf(out, "    <species_distribution>");
    ivector species_distribution;
    ivector_init1(&species_distribution, 128);
    it = list->head;
    while (it != NULL)
    {
        ivector_add(&species_distribution, species_total(it->sp));
        it = it->next;
    }
    ivector_sort_asc(&species_distribution);
    ivector_print(&species_distribution, out);
    fprintf(out, "</species_distribution>\n");

    double *octaves;
    int oct_num = biodiversity_octaves(species_distribution.array, species_distribution.size, &octaves);
    fprintf(out, "    <octaves>");
    for (i = 0; i < oct_num; ++i)
    {
        fprintf(out, "%.2f ", octaves[i]);
    }
    fprintf(out, "%.2f</octaves>\n", octaves[i]);
    fprintf(out, "  </global>\n");

    // Print info on all vertices
    double *restrict ric_per_c = (double*)malloc(communities * sizeof(double));
    for (int c = 0; c < communities; ++c)
    {
        fprintf(out, "  <vertex>\n");
        fprintf(out, "    <id>%d</id>\n", c);
        if (shape[0] == 'r')
        {
            fprintf(out, "    <xcoor>%.4f</xcoor>\n", x[c]);
            fprintf(out, "    <ycoor>%.4f</ycoor>\n", y[c]);
        }
        fprintf(out, "    <degree>%d</degree>\n", g.num_e[c] + 1);
        fprintf(out, "    <speciation_events>%d</speciation_events>\n", speciation_per_c[c]);
        fprintf(out, "    <extinction_events>%d</extinction_events>\n", extinction_per_c[c]);

        int vertex_richess = 0;
        ivector_rmvall(&species_distribution);
        it = list->head;
        while (it != NULL)
        {
            ivector_add(&species_distribution, it->sp->n[c]);
            it = it->next;
        }
        // Sort the species distribution and remove the 0s
        ivector_sort_asc(&species_distribution);
        ivector_trim_small(&species_distribution, 1);

        ric_per_c[c] = (double)species_distribution.size;
        fprintf(out, "    <species_richess>%d</species_richess>\n", species_distribution.size);
        fprintf(out, "    <species_distribution>");
        ivector_print(&species_distribution, out);
        fprintf(out, "</species_distribution>\n");

        // Print octaves
        free(octaves);
        oct_num = biodiversity_octaves(species_distribution.array, species_distribution.size, &octaves);
        fprintf(out, "    <octaves>");
        for (i = 0; i < oct_num - 1; ++i)
        {
            fprintf(out, "%.2f ", octaves[i]);
        }
        fprintf(out, "%.2f</octaves>\n", octaves[i]);
        fprintf(out, "  </vertex>\n");
    }

    fprintf(out, "</simulation>\n");

    // GraphML output:
    sprintf(buffer, "%s%u.graphml", P.ofilename, seed);
    FILE *outgml = fopen(buffer, "w");
    graph_graphml(&g, outgml, seed);
    
    // Print to SVG files.
    sprintf(buffer, "%s%u.svg", P.ofilename, seed);
    FILE *outsvg = fopen(buffer, "w");
    graph_svg(&g, x, y, 400.0, 20.0, outsvg);
    
    sprintf(buffer, "%s%u-speciation.svg", P.ofilename, seed);
    FILE *outsvgspe = fopen(buffer, "w");
    double *spe_per_c = (double*)malloc(communities * sizeof(double));
    for (int c = 0; c < communities; ++c)
    {
        spe_per_c[c] = (double)speciation_per_c[c];
    }
    scale_0_1(spe_per_c, communities);
    graph_svg_abun(&g, x, y, 400.0, 20.0, spe_per_c, 2, outsvgspe);
    
    sprintf(buffer, "%s%u-richness.svg", P.ofilename, seed);
    FILE *outsvgric = fopen(buffer, "w");
    scale_0_1(ric_per_c, communities);
    graph_svg_abun(&g, x, y, 400.0, 20.0, ric_per_c, 1, outsvgric);

    //////////////////////////////////////////////////
    // EPILOGUE...                                  //
    //////////////////////////////////////////////////
    // Close files;
    fclose(out);
    fclose(outgml);
    fclose(outsvg);
    fclose(outsvgspe);
    fclose(outsvgric);
    // Free arrays;
    free(x);
    free(y);
    free(ric_per_c);
    free(spe_per_c);
    free(buffer);
    free(total_species);
    free(octaves);
    free(speciation_per_c);
    free(extinction_per_c);
    free(speciation_events);
    free(extinction_events);
    // Free structs;
    species_list_free(list);
    ivector_free(&species_distribution);
    ivector_free(&lifespan);
    ivector_free(&pop_size);
    graph_free(&g);
    gsl_rng_free(rng);

    return NULL;
}

double **setup_cumulative_list(const graph *g, double omega)
{
    const int num_v = g->num_v;
    double **cumul = (double**)malloc(num_v * sizeof(double*));
    for (int i = 0; i < num_v; ++i) 
    {
        cumul[i] = (double*)malloc(g->num_e[i] * sizeof(double));
    }
    for (int i = 0; i < num_v; ++i)
    {
        for (int j = 0; j < g->num_e[i]; ++j)
        {
            cumul[i][j] = g->w_list[i][j];
        }
    }
    for (int i = 0; i < num_v; ++i)
    {
        for (int j = 0; j < g->num_e[i]; ++j)
        {
            if (i != g->adj_list[i][j])
            {
                cumul[i][j] = omega;
            }
            else
            {
                cumul[i][j] = 1.0;
            }
        }
    }
    for (int i = 0; i < num_v; ++i)
    {
        double sum = 0.0;
        const int num_e = g->num_e[i];
        for (int j = 0; j < num_e; ++j)
        {
            sum += cumul[i][j];
        }
        for (int j = 0; j < num_e; ++j)
        {
            cumul[i][j] /= sum;
        }
    }
    for (int i = 0; i < num_v; ++i)
    {
        const int num_e = g->num_e[i];
        for (int j = 1; j < num_e - 1; ++j)
        {
            cumul[i][j] += cumul[i][j - 1];
        }
        cumul[i][num_e - 1] = 1.0;
    }
    /*
    graph_print(g, stdout);
    for (int i = 0; i < num_v; ++i)
    {
        const int num_e = g->num_e[i];
        for (int j = 0; j < num_e; ++j)
        {
            printf("%.4f   ", cumul[i][j]);
        }
        printf("\n");
    }
    */
    return cumul;
}
