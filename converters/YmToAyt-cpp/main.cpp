/*
 *
 *  ┓┏┳┳┓       ┏┓┓┏┏┳┓
 *  ┗┫┃┃┃  ╋┏┓  ┣┫┗┫ ┃
 *  ┗┛┛ ┗  ┗┗┛  ┛┗┗┛ ┻
 *  by
 *  ┏┓┏┓┏┓   ╻   ┓ ┏┓┏┓┏┓┳┓  ┏┓┓┏┏┓┏┳┓┏┓┳┳┓
 *  ┃┓┃┃┣┫  ━╋━  ┃ ┃┃┃┓┃┃┃┃  ┗┓┗┫┗┓ ┃ ┣ ┃┃┃
 *  ┗┛┣┛┛┗   ╹   ┗┛┗┛┗┛┗┛┛┗  ┗┛┗┛┗┛ ┻ ┗┛┛ ┗
 *
 *  // Proof of Concept by Tronic/GPA
 *  // Format definition and improvements by Longshot & Siko
 *  // C++ Conversion Tool: Siko
 *  // Ultra optimized Z80 Players by Longshot
 *
 *  Build with :
 *    g++ -std=c++17 -O2 -Wall *.cpp -o ym2ayt
 *  Under windows (mingw or cygwin) you might add -static option to avoid a missing DLL error:
 *    g++ -std=c++17 -O2 -Wall *.cpp -o ym2ayt -static
 *  And then:
 *    strip ym2ayt
 *
 *  basic usage:
 *  ./ym2ayt mysong.ym -t cpc
 *
 *  better optimization (slower):
 *  ./ym2ayt mysong.ym -t cpc -O2
 *
 *  check README.md for detailed usage, --help for options
 */

#include "YMParser.h"
#include "ayt.h"
#include "optimizers.h"
#include "platforms.h"

// using namespace std;

// Globals
struct Options options;

// Flag set to true when a slow optimization has started, so it can be interrupted with CTRL+C
bool optimization_running = false;

// I/O helpers
static void writeAllBytes(const filesystem::path& filePath, const ByteBlock& data) {
    ofstream file(filePath, ios::binary);
    if (!file) {
        throw runtime_error("Failed writing file : " + filePath.string());
    }
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<streamsize>(data.size()));
}

static void dumpDb(ofstream& file, const ByteBlock& data, uint8_t groupSize, size_t start = 0,
                   size_t length = 0) {

    if (!file) {
        throw runtime_error("Failed writing file");
    }

    if (length == 0)
        length = data.size() - start;

    for (size_t i = 0; i < length; ++i) {
        if (i % groupSize == 0) {
            file << "  DB ";
        }
        file << "#" << uppercase << hex << setw(2) << setfill('0')
             << static_cast<int>(data[start + i]);
        if ((i + 1) % groupSize == 0) {
            file << endl;
        } else {
            if (i < length - 1) {
                file << ",";
            } else {
                file << endl;
            }
        }
    }
}

// Dump all data stored in a ByteBlock to a text file, using db
// directives
static void dumpAsDb(const filesystem::path& filePath, const ByteBlock& data, uint8_t groupSize) {
    ofstream file(filePath);
    if (!file) {
        throw runtime_error("Failed writing file : " + filePath.string());
    }
    dumpDb(file, data, groupSize);
}

static void saveRawData(const filesystem::path& baseName, const ByteBlock& data,
                        const string& suffix) {
    string prefix = baseName.string() + suffix;
    // writeAllBytes(prefix + ".bin", data);
    dumpAsDb(prefix + ".txt", data, 16);
}

//======================================================================================

// Save raw data
void saveOutputs(const filesystem::path& baseName, const ResultSequences& buffers) {

    // writeAllBytes(baseName.string() + "_patterns.bin", buffers.optimizedOverlap.optimized_heap);
    dumpAsDb(baseName.string() + "_patterns.txt", buffers.optimizedOverlap.optimized_heap,
             16); // converter.patternSize);

    for (size_t regIndex = 0; regIndex < buffers.sequenced.size(); ++regIndex) {

        string prefix = baseName.string() + "_R" + (regIndex < 10 ? "0" : "") + to_string(regIndex);
        // writeAllBytes(prefix + "_sequences.bin", buffers.sequenced[regIndex]);
        dumpAsDb(prefix + "_sequences.txt", buffers.sequenced[regIndex], 16);
    }
}

//======================================================================================
// CLI
//======================================================================================
static void printUsage(const char* prog) {
    cout << "Usage: " << prog << " [options] YM-FILE" << endl
         << "  -h, --help                   Show help" << endl
         << "  -v, --verbose                Increase verbosity" << endl
         << "  -q, --quiet                  Low verbosity" << endl
         << "  -o, --output FILE            Name of the output file (works only if there is a "
            "single file)"
         << endl
         << "  -P, --output-path PATH       Folder where to store results files (must exist)"
         << endl
         << "  -s, --save-regs              Save intermediary files (raw regs register dumps)"
         << endl
         << "  -S, --save-size              Add actual file size in output file" << endl
         << "  -p, --pattern-size N[:N][/N] Pattern search range (presets: 'full', 'auto')" << endl
         << "  -l, --only-evenly-looping    Skip sizes that don't loop at thebeginning of a pattern"
         << endl
         << "  -sp, --scale-period F        Multiply tone periods (R0..R5) by F (e.g., 0.5,2)"
         << endl
         << "  -sn, --scale-noise F         Multiply tone periods (R0..R5) by F (e.g., 0.5,2)"
         << endl
         << "  -sv, --scale-envelope F      Multiply tone periods (R0..R5) by F (e.g., 0.5,2)"
         << endl
         << "  -t, --target ARCH            Name of the target architecture ("
         << generate_platform_list() << ")" << endl
         << "  -c, --csv                    Export stats in CSV format" << endl
         << "  -R, --export--all-regs       Force exporting all registers, even constant ones"
         << endl
         << "  -O, --overlap-optim N        Overlap optimization method (none, ga, tabu, "
            "sa, ifs)"
         << endl
         << "   --fur-filter                Apply repeated values fix in R13 sequences" << endl
         << "  --ga-pop-size M L            Size of population Mu and Lambda" << endl
         << "  --ga-gen-min N               Minimum number of generations" << endl
         << "  --ga-gen-max N               Maximum number of generations" << endl
         << "  --ga-gen-ext N               When finding a new solution, extends number of "
            "generation, for further search"
         << endl;
}

/**
 * @brief Parse le paramètre de taille de motif (Min[:Max][/Step]) et met à jour les options.
 * @param size_str La chaîne de l'argument (e.g., "4", "4:64", "4:64/4", "full", "auto").
 * @param options La structure Options à mettre à jour.
 * @return true si l'analyse est réussie, false sinon.
 */
bool parsePatternSize(const string& size_str, Options& options) {
    string s = size_str;

    if (s == "full") {
        options.patternSizeMin = 1;
        options.patternSizeMax = 128;
        options.patternSizeStep = 1;
        return true;
    }
    if (s == "auto") {
        options.patternSizeMin = 8;
        options.patternSizeMax = 96;
        options.patternSizeStep = 4;
        return true;
    }

    options.sizeParam = s; // Sauvegarder la valeur brute

    try {
        size_t colon_pos = s.find(':');
        size_t slash_pos = s.find('/');

        // 1. Gérer le Step (s'il existe)
        string step_str = "1";
        if (slash_pos != string::npos) {
            step_str = s.substr(slash_pos + 1);
            s = s.substr(0, slash_pos);
        }
        int step = stoi(step_str);
        if (step < 1) {
            cerr << "Pattern Size Step must be >= 1" << endl;
            return false;
        }
        options.patternSizeStep = step;

        // 2. Gérer Min et Max
        if (colon_pos == string::npos) {
            // Un seul nombre N -> Min=N, Max=N
            int n = stoi(s);
            if (n < 1 || n > 256) {
                cerr << "Pattern Size must be 1..256" << endl;
                return false;
            }
            options.patternSizeMin = n;
            options.patternSizeMax = n;
        } else {
            // Deux nombres Min:Max
            string min_str = s.substr(0, colon_pos);
            string max_str = s.substr(colon_pos + 1);

            int min_val = stoi(min_str);
            int max_val = stoi(max_str);

            if (min_val < 1 || max_val > 256 || min_val > max_val) {
                cerr << "Pattern Size Min/Max must be 1..256 and Min <= Max" << endl;
                return false;
            }
            options.patternSizeMin = min_val;
            options.patternSizeMax = max_val;
        }

        return true;

    } catch (...) {
        cerr << "Invalid pattern size format: " << size_str << ". Expected: N, N:M, or N:M/S"
             << endl;
        return false;
    }
}

double parseDouble(const string& s) { return stod(s); }
int parseInt(const string& s) { return stoi(s); }
string parseString(const string& s) { return s; }

template <typename T>
bool parseOptionArgument(const vector<string>& optionNames, T Options::*member, Options& options,
                         char** argv, int& i, int argc, T (*converter)(const string&)) {
    const string& arg = argv[i];
    // Vérifie si l'argument actuel correspond à une des options attendues
    bool isOption = false;
    for (const auto& name : optionNames) {
        if (arg == name) {
            isOption = true;
            break;
        }
    }
    if (!isOption) {
        return false; // Ce n'est pas cette option, on passe
    }

    // Parsing de l'argument
    if (i + 1 >= argc) {
        cerr << "Option " << arg << " requires an argument" << endl;
        return false;
    }
    try {
        options.*member = converter(argv[++i]);
        if (options.verbosity > 1)
            cout << "Option " << arg << "=" << options.*member << endl;
        return true;
    } catch (...) {
        cerr << "Invalid argument for " << arg << endl;
        return false;
    }
}

static uint8_t reverse_bits(uint8_t byte) {
    uint8_t reversed = 0;
    for (int i = 0; i < 8; ++i) {
        uint8_t bit = (byte >> i) & 1;
        reversed |= (bit << (7 - i));
    }
    return reversed;
}

//======================================================================================
// Main program
//======================================================================================

#ifndef TEST_MODE

static void signal_handler(int sig) {
    // cout << "Signal " << sig << " opt running:" << optimization_running;
    if (optimization_running == true)
        optimization_running = false;
    else
        exit(1);
}

int main(int argc, char** argv) {

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // string outputFileName = "";
    vector<string> inputPaths;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--verbose") {
            options.verbosity++;
            continue;
        }
        if (arg == "-q" || arg == "--quiet") {
            options.verbosity = 0;
            continue;
        }
        if (arg == "-s" || arg == "--save-regs") {
            options.saveFiles = true;
            continue;
        }

        if (arg == "-S" || arg == "--save-size") {
            options.save_size = true;
            continue;
        }
        if (arg == "-l" || arg == "--only-evenly-looping") {
            options.onlyDivisible = true;
            continue;
        }
        if (arg == "-c" || arg == "--csv") {
            options.exportCsv = true;
            continue;
        }
        if (arg == "-R" || arg == "--export-all-regs") {
            options.exportAllRegs = true;
            continue;
        }

        if (arg == "--fur-filter") {
            options.filterReg13 = true;
            continue;
        }

        if (arg=="--useSilenceMasking") {
            options.useSilenceMasking = true;
            continue;
        }
 
        if (arg == "-x" || arg == "--extraFinalSequence") {
            options.extraFinalSequence = true;
            continue;
        }

        // Shortcuts
        if (arg == "-O0") {
            options.optimizationLevel = 0;
            continue;
        }

        if (arg == "-O1") {
            options.optimizationLevel = 1;
            continue;
        }

        if (arg == "-O2") {
            options.optimizationMethod = "sa";
            options.optimizationLevel = 2;
            continue;
        }

        if (arg == "-O3") {
            options.optimizationMethod = "ga";
            options.optimizationLevel = 2;
            options.patternSizeMin = 2;
            options.patternSizeMax = 128;
            continue;
        }

        /* ------------------------------------------------------------------------------------------------
         */
        /* Options nécessitant un argument (ou plus) */

        else if (arg == "-t" || arg == "--target") {
            if (i + 1 >= argc) {
                cerr << "Option " << arg << " requires an argument" << endl;
                return 1;
            }

            options.targetParam = argv[++i];

            const PlatformFreq& pf = find_frequency_by_platform(options.targetParam);
            options.targetClock = pf.freq;
            options.targetId = pf.id;
            if (options.targetClock < 0) {
                cerr << "Option -t argument must be a known platform (e.g., ATARI, CPC, ...)"
                     << endl;
                return 1;
            }
            continue;
        } else if (arg == "-p" || arg == "--pattern-size") {
            if (i + 1 >= argc) {
                cerr << "Option " << arg << " requires an argument" << endl;
                return 1;
            }
            if (!parsePatternSize(argv[++i], options)) {
                return 1; // Erreur gérée dans parsePatternSize
            }
            continue;
        }

        if (parseOptionArgument({"-sp", "--scale-period"}, &Options::periodCoef, options, argv, i,
                                argc, &parseDouble)) {
            if (options.periodCoef <= 0.0) {
                cerr << "Period coefficient must be > 0" << endl; // exception?
                return 1;
            }
            continue;
        }

        if (parseOptionArgument({"-sn", "--scale-noise"}, &Options::noiseCoef, options, argv, i,
                                argc, &parseDouble)) {
            if (options.noiseCoef <= 0.0) {
                cerr << "Envelope coefficient must be > 0" << endl; // exception?
                return 1;
            }
            continue;
        }

        if (parseOptionArgument({"-se", "--scale-envelope"}, &Options::envCoef, options, argv, i,
                                argc, &parseDouble)) {
            if (options.envCoef <= 0.0) {
                cerr << "Envelope coefficient must be > 0" << endl; // exception?
                return 1;
            }
            continue;
        }

        if (parseOptionArgument({"-O", "--overlap-optim"}, &Options::optimizationMethod, options,
                                argv, i, argc, &parseString)) {

            if (options.optimizationMethod == "none") {
                options.optimizationLevel = 0;
                continue;
            }
            if (options.optimizationMethod == "ga") {
                options.optimizationLevel = 2;
                continue;
            }
            if (options.optimizationMethod == "sa") {
                options.optimizationLevel = 2;
                continue;
            }
            if (options.optimizationMethod == "ils") {
                options.optimizationLevel = 2;
                continue;
            }
            if (options.optimizationMethod == "tabu") {
                options.optimizationLevel = 2;
                continue;
            }
            if (options.optimizationMethod == "glutonny") {
                options.optimizationLevel = 1;
                continue;
            }
            cerr << "Unknown optimization method:" << options.optimizationMethod;
            return 1;
        }

        if (parseOptionArgument({"-o", "--output-file"}, &Options::outputFile, options, argv, i,
                                argc, parseString)) {
            continue;
        }
        if (parseOptionArgument({"-P", "--output-path"}, &Options::outputPath, options, argv, i,
                                argc, parseString)) {
            continue;
        }

        /* Options GA */
        if (arg == "--ga-pop-size") {
            if (i + 2 >= argc) {
                cerr << "Option " << arg << " requires two arguments: Mu and Lambda" << endl;
                return 1;
            }
            try {
                options.ga_MU = stoul(argv[++i]);
                options.ga_LAMBDA = stoul(argv[++i]);
            } catch (...) {
                cerr << "Invalid population sizes for " << arg << endl;
                return 1;
            }
            if (options.ga_MU < 1 || options.ga_LAMBDA < 1) {
                cerr << "Population sizes must be >= 1" << endl;
                return 1;
            }
            continue;
        }

        if (parseOptionArgument({"--ga-gen-min"}, &Options::ga_NUM_GENERATION_MIN, options, argv, i,
                                argc, &parseInt)) {
            continue;
        }

        if (parseOptionArgument({"--ga-gen-max"}, &Options::ga_NUM_GENERATION_MAX, options, argv, i,
                                argc, &parseInt)) {
            continue;
        }

        if (parseOptionArgument({"--ga-gen-ext"}, &Options::ga_ADDITIONAL_GENERATIONS, options,
                                argv, i, argc, &parseInt)) {
            continue;
        }

        if (!arg.empty() && arg[0] != '-') {
            inputPaths.push_back(arg);
        } else {
            cerr << "Unknown option: " << arg << endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    //--  Options parsing finished

    if (inputPaths.empty()) {
        cerr << "Error: No input FILE specified." << endl;
        printUsage(argv[0]);
        return 1;
    }

    if (inputPaths.size() > 1 && (!options.outputFile.empty())) {
        cerr << "Error: Cannot use -o with multiple files.Maybe you can use -P instead?" << endl;
        printUsage(argv[0]);
        return 1;
    }

    for (auto path : inputPaths) {
        try {
            filesystem::path inputPath = path;

            if (options.verbosity > 0)
                cout << "Opening " << inputPath << endl;

            ifstream file(inputPath, ios::binary);
            if (!file) {
                throw runtime_error("Unable to open YM6 file: " + inputPath.string());
            }

            YMData ymdata;

            uint32_t frameCount = ymdata.readHeader(file);

            if (frameCount == 0) {
                throw runtime_error("YM File contains no frame");
            }
            if (options.verbosity > 0)
                ymdata.header.print();

            ymdata.readFrames(file);

            // Check for potential anomalies in reg13 sequence
            int r13_suspicious_values = ymdata.filterReg13(options.filterReg13);
            if (r13_suspicious_values > 0) {
                if (options.filterReg13) {
                    cerr << path << ": R13: Replaced " << r13_suspicious_values
                         << " repeated values" << endl;
                } else {
                    cerr << "/!\\ Warning! " << path << ": Detected " << r13_suspicious_values
                         << " repeated values in R13 sequence. Try --fur-filter if you encounter "
                            "issues."
                         << endl;
                }
            }

            // Actual coefficients
            double periodCoef = options.periodCoef;
            double noiseCoef = options.noiseCoef;
            double envCoef = options.envCoef;
            

            // Computes automatically coefficients to appy to registers, depending on YM masterClock
            // and target platform
            if (options.targetClock > 0) {
                if (options.periodCoef < 0)
                    periodCoef = options.targetClock / ymdata.header.masterClock;
                if (options.noiseCoef < 0)
                    noiseCoef = options.targetClock / ymdata.header.masterClock;
                if (options.envCoef < 0)
                    envCoef = options.targetClock / ymdata.header.masterClock;
            }

            if (options.verbosity > 0) {
                cout << "Period Coef = " << periodCoef << endl;
                cout << "Noise Coef = " << noiseCoef << endl;
                cout << "Envelop Coef = " << envCoef << endl;
            }

            filesystem::path baseName = inputPath.stem();
            int patternSize = 0;

            ResultSequences finalBuffers;

            AYTConverter converter;

            // Apply tone period multiplier (R0..R5)
            if (periodCoef > 0) {
                if (options.verbosity)
                    cout << "Applying tone period scaling: x" << periodCoef << endl;
                ymdata.scalePeriods(periodCoef, options.verbosity > 1);
            }

            if (noiseCoef > 0) {
                if (options.verbosity)
                    cout << "Applying noise scaling: x" << noiseCoef << endl;
                ymdata.scaleNoise(noiseCoef);
            }

            if (envCoef > 0) {
                if (options.verbosity)
                    cout << "Applying envelope scaling: x" << envCoef << endl;
                ymdata.scaleEnvelope(envCoef);
            }

            // Determine number of registers and fixed values
            converter.activeRegs = analyze_data_buffers(
                ymdata.rawRegisters, converter.initRegValues, options.exportAllRegs);

            // devrait etre calculé avant
            int numRegs = 0;
            for (int r = 0; r < 14; ++r) {
                // Check if register is exported
                if (converter.activeRegs & (1 << r)) {
                    numRegs += 1;
                }
            }

            // If not dividing evenly, we add final sequence to rawRegisters
            auto rawRegistersWithEndSequence = ymdata.rawRegisters;
            for (int i = 0; i < 16; i++) {
                rawRegistersWithEndSequence[i].push_back(final_sequence_values[final_sequence[i]]);
            }

            // If dividing evenly, we can create an additional sequence, or replace last values with
            // special sequence, depending on options.addFinalSequence
            if (!options.extraFinalSequence) {
                if (options.verbosity > 0)
                    cout << "Changing End of raw registers, replacing with final sequence values"
                         << endl;
                for (int i = 0; i < 16; i++) {
                    uint8_t v = final_sequence_values[final_sequence[i]];
                    size_t s = ymdata.rawRegisters[i].size();
                    // if (v != 0) {
                    ymdata.rawRegisters[i][s - 1] = v;
                    //}
                }
            }

            size_t bestTotal = static_cast<size_t>(-1);
            int bestSize = -1;

            RegisterValues& selRegValueSet = ymdata.rawRegisters;

            for (int s = options.patternSizeMax; s >= options.patternSizeMin;
                 s -= options.patternSizeStep) {
                bool valid = true;
                bool isDividingEvenly = (ymdata.header.frameCount % s) == 0;

                RegisterValues& regValueSet = ymdata.rawRegisters;
                if (!isDividingEvenly)
                    regValueSet = rawRegistersWithEndSequence;

                size_t currentTotal = 0;

                ResultSequences currentBuffers;

                if ((options.onlyDivisible == true) && !isDividingEvenly) {
                    valid = false;
                } else {
                    if (options.verbosity > 2)
                        cout << "Testing Pattern Size " << s << " :";

                    try {

                        currentBuffers = buildBuffers(regValueSet, converter.activeRegs, s,
                                                      options.optimizationLevel,options.useSilenceMasking);

                        currentTotal = currentBuffers.optimizedOverlap.optimized_heap.size();

                        for (size_t i = 0; i < currentBuffers.sequenced.size(); ++i) {
                            currentTotal += currentBuffers.sequenced[i].size();
                        }

                    } catch (const exception& e) {
                        if (options.verbosity > 2)
                            cerr << "Exception " << e.what() << endl;
                        valid = false;
                    }

                    if (valid) {
                        if (options.verbosity > 2)
                            cout << " " << currentTotal << endl;

                        if (currentTotal < bestTotal) {
                            bestTotal = currentTotal;
                            bestSize = s;
                            finalBuffers = currentBuffers;
                            selRegValueSet = regValueSet;

                            if (options.verbosity > 0)
                                cout << "New Best Pattern size found: " << s
                                     << " (Total Size: " << currentTotal << " bytes)"
                                     << (isDividingEvenly ? "" : " (info: not evenly dividing)")
                                     << endl;
                        }
                    }
                } // currentBuffers est detruit ici
            }

            if (bestSize == -1) {
                cerr << "Exception: Unable to find a valid pattern size" << endl;
                return 1;
            }

            patternSize = bestSize;

            // Save Raw register patterns
            if (options.saveFiles) {

                for (size_t r = 0; r < selRegValueSet.size(); ++r) {
                    saveRawData(baseName, selRegValueSet[r],
                                string("_Raw_R") + (r < 10 ? "0" : "") + to_string(r));
                }
            }

            if (options.optimizationLevel > 1) {

                // Amélioration de l'ordre par stratégie d'évolution
                optimization_running = true;
                OptimizedResult final_result;

                if (options.optimizationMethod == "ga") {
                    final_result = refine_order_with_evolutionary_algorithm(
                        finalBuffers.optimizedOverlap, finalBuffers.patternMap, patternSize,
                        options, optimization_running);
                }

                if (options.optimizationMethod == "sa") {
                    final_result = refine_order_with_simulated_annealing(
                        finalBuffers.optimizedOverlap, finalBuffers.patternMap, patternSize,
                        options, optimization_running);
                }
                if (options.optimizationMethod == "ils") {
                    final_result = refine_order_with_ils(
                        finalBuffers.optimizedOverlap, finalBuffers.patternMap,
                        100, // Ex: nb patterns **2 // 10 000 itérations
                        patternSize, optimization_running);
                }
                if (options.optimizationMethod == "tabu") {
                    final_result = refine_order_with_tabu_search(finalBuffers.optimizedOverlap,
                                                                 finalBuffers.patternMap, 50,
                                                                 patternSize, optimization_running);
                }

                finalBuffers.optimizedOverlap = final_result;
            }

            replaceByOptimizedIndex(finalBuffers.sequenced, finalBuffers.optimizedOverlap);

            // Interlacing sequences
            if (options.verbosity > 1)
                cout << "\nInterlacing " << finalBuffers.sequenced.size() << " sequence buffers";

            ByteBlock interleavedData = interleaveSequenceBuffers(finalBuffers.sequenced);

            // Sauvegarde des fichiers intermediaires si demandé => dans le _ayt.asm
            if (options.saveFiles) {
                saveOutputs(baseName, finalBuffers);
                cout << "Intermediary files saved for patternSize " << patternSize << endl;
            }

            bool isDividingEvenly = (ymdata.header.frameCount % patternSize) == 0;
            bool isLoopingEvenly = (ymdata.header.loopFrame % patternSize) == 0;

            if (options.verbosity > 0) {
                cout << " Active Regs: " << numRegs << " Flags=0x" << hex << converter.activeRegs
                     << dec << endl;
                cout << " Pattern Length: " << patternSize << endl;
                cout << " Unique patterns" << finalBuffers.num_patterns << endl;

                cout << " Init Sequence: ";
                for (const auto& pair : converter.initRegValues) {
                    cout << pair.first << "=" << static_cast<int>(pair.second) << " ";
                }
                cout << endl;

                cout << " Loop to sequence : " << (ymdata.header.loopFrame / patternSize);
                if (!isLoopingEvenly) {
                    cout << " (Warn: not looping to the start of a pattern) "
                         << ymdata.header.loopFrame << "/" << patternSize;
                }
                cout << endl;

                cout << " Patterns size : " << finalBuffers.optimizedOverlap.optimized_heap.size()
                     << " bytes" << endl;
                cout << " Sequences size : " << interleavedData.size() << " bytes" << endl;
            }

            // Export AYTs
            ByteBlock ayt_header;
            ayt_header.resize(14);

            ByteBlock ayt_file;
            uint16_t offset_sequences =
                ayt_header.size() + finalBuffers.optimizedOverlap.optimized_heap.size();
            uint16_t offset_loop_sequence =
                offset_sequences + (ymdata.header.loopFrame / patternSize) * 2 * numRegs;

            // Total number of pointers, including final sequence
            int addFinalSequence = 0;
            if (isDividingEvenly && options.extraFinalSequence) {
                addFinalSequence = 1;
            }

            uint16_t seqTotalSize = (interleavedData.size()>>1) + addFinalSequence*numRegs; //
            // 
            // Should be equal to 
            //finalBuffers.sequenced.size() * (addFinalSequence + (finalBuffers.sequenced[0].size() >> 1));

            ayt_header[0] = (2 << 4); // Version 2.0
            //  Ayt_ActiveRegs: active reg (bit 2:reg 13...bit 15:reg 0), encoded in Big endian
            ayt_header[1] = reverse_bits((converter.activeRegs >> 8) & 255); // Active Regs R13-R8
            ayt_header[2] = reverse_bits(converter.activeRegs & 255);        // Active Regs R7-R0

            // uint8_t Ayt_PatternSize
            ayt_header[3] = patternSize; 
            // Pointer to Sequence block 
            ayt_header[4] = uint8_t(offset_sequences & 0xFF);
            ayt_header[5] = uint8_t((offset_sequences >> 8) & 0xFF);
            // Pointer to Loop Sequence 
            ayt_header[6] = uint8_t(offset_loop_sequence & 0xFF);
            ayt_header[7] = uint8_t((offset_loop_sequence >> 8) & 0xFF);

            // Nb of pattern pointer (NbSeq x NbReg)
            ayt_header[10] = seqTotalSize & 0xff;
            ayt_header[11] = (seqTotalSize >> 8) & 0xff;

            // Platform/Freq (default 0 = cpc+50hz)
            ayt_header[12] = options.targetId | (options.targetClockId << 5);
            ayt_header[13] = 0; // Reserved

            // Reserve space
            ayt_file.resize(ayt_header.size() + interleavedData.size() +
                            finalBuffers.optimizedOverlap.optimized_heap.size());

            copy(ayt_header.begin(), ayt_header.end(), ayt_file.begin());

            // Append patterns
            copy(finalBuffers.optimizedOverlap.optimized_heap.begin(),
                 finalBuffers.optimizedOverlap.optimized_heap.end(),
                 ayt_file.begin() + ayt_header.size());

            // Append interleaved sequences
            copy(interleavedData.begin(), interleavedData.end(),
                 ayt_file.begin() + offset_sequences);

            if (isDividingEvenly && options.extraFinalSequence) {
                cout << "Creating Additional Sequence" << endl;
                // TODO: Adding Final sequence to add to interleaved Data
                // If dividing evenly, and if final sequence was not inserted, then we must add
                // a final sequence
                // TODO: search inside ayt file for specific data
                // Right now we simply add these 3 spacial bytes at the end of the file
                uint16_t ptr_final_sequence[3];
                for (int k = 0; k < 3; k++)
                    ptr_final_sequence[k] = ayt_file.size() - ayt_header.size() + numRegs * 2 + k;

                for (int r = 0; r < 14; ++r) {
                    // Check if register is exported
                    if (converter.activeRegs & (1 << r)) {
                        uint16_t ptr = ptr_final_sequence[final_sequence[r]];
                        ayt_file.push_back(ptr & 255);
                        ayt_file.push_back(ptr >> 8);
                    }
                }

                // push final sequence special values
                ayt_file.push_back(0x00);
                ayt_file.push_back(0x3F);
                ayt_file.push_back(0xBF);
            }

            // init sequence
            // 2 cases: if empty, we can take any byte with bit 7 set
            // otherwise we create the table using inactive registers values
            uint16_t pointerToInitTable = ayt_file.size();
            ayt_file[8] = pointerToInitTable & 0xff;
            ayt_file[9] = (pointerToInitTable >> 8) & 0xff;
            ayt_header[8] = pointerToInitTable & 0xff;
            ayt_header[9] = (pointerToInitTable >> 8) & 0xff;

            if ((converter.activeRegs & 0x3fffu) != 0x3fffu) {
                for (const auto& pair : converter.initRegValues) {
                    ayt_file.push_back(pair.first);
                    ayt_file.push_back(pair.second);
                }
            }

            ayt_file.push_back(0xFF);

            string flags = "";
            if (!options.targetParam.empty())
                flags += "-" + options.targetParam;
            if (options.filterReg13)
                flags += "-fr13";
            if (options.useSilenceMasking)
                flags += "-sopt";
            if (options.save_size)
                flags += "-" + to_string(ayt_file.size());

            string ayt_filename = options.outputFile;
            if (ayt_filename.empty())
                ayt_filename = baseName.string() + flags + ".ayt";

            filesystem::path full_path = filesystem::path(options.outputPath) / ayt_filename;

            writeAllBytes(full_path.string(), ayt_file);

            if (options.verbosity > 0) {
                cout << " Total File size: " << ayt_file.size() << endl;
                cout << "AYT file \"" << full_path.string() << "\" saved." << endl;
            }

            // CSV stats

            if (options.exportCsv == true) {
                cout << baseName << ";" << ymdata.header.masterClock << ";"
                     << ymdata.header.frequency << ";" << ymdata.header.frameCount << ";"
                     << ((double)ymdata.header.frameCount) / ymdata.header.frequency << ";"
                     << ymdata.header.loopFrame << ";" << finalBuffers.sequenced.size() <<";"
                   << hex << converter.activeRegs << dec  ;
                
                   for (int i = 13; i >= 0; i--) {
                    cout << ";" << ((converter.activeRegs >> i) & 1);
                }
                cout << ";" << patternSize << ";" << finalBuffers.num_patterns << ";"
                     << isDividingEvenly << ";" << isLoopingEvenly << ";" << r13_suspicious_values
                     << ";" << finalBuffers.optimizedOverlap.optimized_heap.size() << ";"
                     << interleavedData.size() << ";" << ayt_file.size() << ";" << endl;
            }
        } catch (const exception& e) {
            cerr << "Exception: " << e.what() << endl;
            return 1;
        }
    }

    return 0;
}
#endif
