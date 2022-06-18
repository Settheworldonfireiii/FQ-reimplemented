#include <iostream>
#include "barcode_utils.hpp"
#include "datamatrix_utils.hpp"
#include "dirent.h"


#include "ReadParser.h"


using namespace std;



struct InputFile {
    std::string input_r1, input_r2;

    InputFile(std::string r1, std::string r2) : input_r1(r1), input_r2(r2) {}
};
vector<InputFile> inputs;

struct result_t {
    int collector_pos;
    int cell_id;
    uint64_t umi;
    int feature_id;

    result_t(int a1, int a2, uint64_t a3, int a4) : collector_pos(a1), cell_id(a2), umi(a3), feature_id(a4) {}
};



inline string safe_substr(const string& sequence, size_t pos, size_t length) {
    if (pos + length > sequence.length()) {
        printf("Error: Sequence length %zu is too short (expected to be at least %zu)!\n", sequence.length(), pos + length);
        exit(-1);
    }
    return sequence.substr(pos, length);
}

void parse_feature_names(int n_feature, std::vector<std::string>& feature_names, int& n_cat, std::vector<std::string>& cat_names, std::vector<int>& cat_nfs, std::vector<int>& feature_categories) {
    size_t pos;
    std::string cat_str;

    n_cat = 0;

    pos = feature_names[0].find_first_of(',');
    if (pos != std::string::npos) {
        cat_names.clear();
        cat_nfs.clear();
        feature_categories.resize(n_feature, 0);
        for (int i = 0; i < n_feature; ++i) {
            pos = feature_names[i].find_first_of(',');
            assert(pos != std::string::npos);
            cat_str = feature_names[i].substr(pos + 1);
            feature_names[i] = feature_names[i].substr(0, pos);
            if (n_cat == 0 || cat_names.back() != cat_str) {
                cat_names.push_back(cat_str);
                cat_nfs.push_back(i);
                ++n_cat;
            }
            feature_categories[i] = n_cat - 1;
        }
        cat_nfs.push_back(n_feature);
    }
}




void parse_input_directory(char* input_dirs) {
    DIR *dir;
    struct dirent *ent;
    vector<string> mate1s, mate2s;

    string mate1_pattern = string("R1_001.fastq.gz");
    string mate2_pattern = string("R2_001.fastq.gz");
    string dir_name;

    char *input_dir = strtok(input_dirs, ",");

    inputs.clear();
    while (input_dir != NULL) {
        assert((dir = opendir(input_dir)) != NULL);

        dir_name = std::string(input_dir) + "/";

        mate1s.clear();
        mate2s.clear();

        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                std::string file_name = std::string(ent->d_name);
                size_t pos;

                pos = file_name.find(mate1_pattern);
                if (pos != std::string::npos && pos + mate1_pattern.length() == file_name.length()) {
                    mate1s.push_back(file_name);
                }

                pos = file_name.find(mate2_pattern);
                if (pos != std::string::npos && pos + mate2_pattern.length() == file_name.length()) {
                    mate2s.push_back(file_name);
                }
            }
        }

        size_t s = mate1s.size();

        assert(s == mate2s.size());
        sort(mate1s.begin(), mate1s.end());
        sort(mate2s.begin(), mate2s.end());

        for (size_t i = 0; i < s; ++i) {
            inputs.emplace_back(dir_name + mate1s[i], dir_name + mate2s[i]);
        }

        input_dir = strtok(NULL, ",");
    }
}

int main(int argc, char* argv[]) {



    uint32_t nt; // fastx_parser consumer threads
    uint32_t np; // fastx_parser producer threads

    int max_mismatch_cell, max_mismatch_feature;
    size_t umi_len;
    std::string feature_type, totalseq_type, scaffold_sequence;
    int barcode_pos; // Antibody: Total-Seq A 0; Total-Seq B or C 10. Crispr: default 0, can be set by option
    bool convert_cell_barcode;

    time_t start_time, end_time;

    std::vector<InputFile> inputs;

    int n_cell, n_feature; // number of cell and feature barcodes
    size_t cell_blen, feature_blen; // cell barcode length and feature barcode length
    std::vector<std::string> cell_names, feature_names;
    HashType cell_index, feature_index;



    int f[2][7]; // for banded dynamic programming, max allowed mismatch = 3

    int n_cat; // number of feature categories (e.g. hashing, citeseq)
    std::vector<std::string> cat_names; // category names
    std::vector<int> cat_nfs, feature_categories; // cat_nfs, number of features in each category; int representing categories.
    vector<DataCollector> dataCollectors;

    start_time = time(NULL);

    max_mismatch_cell = 1;
    feature_type = "antibody";
    max_mismatch_feature = 3;
    umi_len = 10;
    barcode_pos = -1;
    totalseq_type = "";
    scaffold_sequence = "";
    convert_cell_barcode = false;
    nt = 1;
    np = 1;

    for (int i = 5; i < argc; ++i) {
        if (!strcmp(argv[i], "--max-mismatch-cell")) {
            max_mismatch_cell = atoi(argv[i + 1]);
        }
        if (!strcmp(argv[i], "--feature")) {
            feature_type = argv[i + 1];
        }
        if (!strcmp(argv[i], "--max-mismatch-feature")) {
            max_mismatch_feature = atoi(argv[i + 1]);
        }
        if (!strcmp(argv[i], "--umi-length")) {
            //std::cout<<"DXDD"<<std::endl;
            umi_len = atoi(argv[i + 1]);
        }
        if (!strcmp(argv[i], "--barcode-pos")) {
            barcode_pos = atoi(argv[i + 1]);
        }
        if (!strcmp(argv[i], "--convert-cell-barcode")) {
            convert_cell_barcode = true;
        }
        if (!strcmp(argv[i], "--scaffold-sequence")) {
            scaffold_sequence = argv[i + 1];
        }
        if (!strcmp(argv[i], "--nt")) {
            nt = atoi(argv[i + 1]);
        }
        if (!strcmp(argv[i], "--np")) {
            np = atoi(argv[i + 1]);
        }
    }

    printf("Load feature barcodes.\n");
    parse_sample_sheet(argv[2], n_feature, feature_blen, feature_index, feature_names, max_mismatch_feature);
    parse_feature_names(n_feature, feature_names, n_cat, cat_names, cat_nfs, feature_categories);

    parse_input_directory(argv[3]);

  /*  if (feature_type == "antibody") {
        if (barcode_pos < 0) detect_totalseq_type(); // if specify --barcode-pos, must be a customized assay
    }
    else {
        if (feature_type != "crispr") {
            printf("Do not support unknown feature type %s!\n", feature_type.c_str());
            exit(-1);
        }
        if (barcode_pos < 0) barcode_pos = 0; // default is 0
    }*/

    printf("Load cell barcodes.\n");
    convert_cell_barcode = convert_cell_barcode || (feature_type == "antibody" && totalseq_type == "TotalSeq-B");
    parse_sample_sheet(argv[1], n_cell, cell_blen, cell_index, cell_names, max_mismatch_cell, convert_cell_barcode);
    printf("Time spent on parsing cell barcodes = %.2fs.\n", difftime(time(NULL), start_time));




    dataCollectors.resize(n_cat > 0 ? n_cat : 1);

    std::vector<std::vector<std::string>> inputss;
    //std::vector<std::string> inputss;
    for (auto&& input_fastq : inputs) {
        inputss[0].push_back(input_fastq.input_r1);
        inputss[0].push_back(input_fastq.input_r2);
    }

    ReadParser parser(inputss, nt);
    parser.start();
    int cnt = 0;
    mutex work_mutex;
    vector<thread> readers;
    for (size_t i = 0; i < nt; ++i) {
        readers.emplace_back([&, i]() {
            std::cout<<"are we here 237 main"<<std::endl;
            string cell_barcode, umi, feature_barcode;
            uint64_t binary_cell, binary_umi, binary_feature;
            size_t read1_len;
            int feature_id, collector_pos;

            HashIterType cell_iter, feature_iter;
            vector<result_t> results;

            int thread_read_cnt = 0;
            auto rg = parser.getReadGroup();
            //std::cout<<rg.
            std::cout<<"are we here 249 main"<<std::endl;
            //std::cout<<int(rg.size())<<std::endl;

            while (parser.refill(rg)) {
                // should be while (parser.refill(rg)) {

                std::cout<<"are we here 252 main"<<std::endl;

               for (auto& seqPair : rg) {
                    std::cout<<"are we here 255 main"<<std::endl;

                    auto& read1 = seqPair;
                    std::cout<<" JE "<<read1.seq<<"  ICI"<<std::endl;
                    //auto& read2 = seqPair.second;
                    //std::cout<<" SUIS "<<read2.seq.l<<"  ICI"<<std::endl;

                    ++thread_read_cnt;

                    cell_barcode = safe_substr(read1.seq, 0, cell_blen);
                    binary_cell = barcode_to_binary(cell_barcode);
                    cell_iter = cell_index.find(binary_cell);

                    if (cell_iter != cell_index.end() && cell_iter->second.item_id >= 0) {
                        //if (extract_feature_barcode(read2.seq.s, feature_blen, feature_type, feature_barcode)) {
                            binary_feature = barcode_to_binary(feature_barcode);
                            feature_iter = feature_index.find(binary_feature);
                            if (feature_iter != feature_index.end() && feature_iter->second.item_id >= 0) {
                                read1_len = read1.size();
                                if (read1_len < cell_blen + umi_len) {
                                    printf("Warning: Detected read1 length %zu is smaller than cell barcode length %zu + UMI length %zu. Shorten UMI length to %zu!\n", read1_len, cell_blen, umi_len, read1_len - cell_blen);
                                    umi_len = read1_len - cell_blen;
                                }
                                umi = safe_substr(read1.seq, cell_blen, umi_len);
                                binary_umi = barcode_to_binary(umi);
                                std::cout<<"are we here 273 main"<<std::endl;
                                feature_id = feature_iter->second.item_id;
                                collector_pos = n_cat > 0 ? feature_categories[feature_id] : 0;

                                results.emplace_back(collector_pos, cell_iter->second.item_id, binary_umi, feature_id);
                            }
                       // }
                    }
                }
                std::cout<<"are we here 290 main"<<std::endl;

            }
            std::cout<<"are we here 293 main"<<std::endl;

            work_mutex.lock();
            for (result_t& r : results) {
                dataCollectors[r.collector_pos].insert(r.cell_id, r.umi, r.feature_id);
            }
            cnt += thread_read_cnt;
            printf("Thread %zu processed %d reads. Total %d.\n", i, thread_read_cnt, cnt);
            work_mutex.unlock();
        });
    }
    std::cout<<"are we here 292 main"<<std::endl;

    for (auto& t : readers) {
        std::cout<<"are we here 297 main"<<std::endl;

        t.join();
        std::cout<<"are we here 300 main"<<std::endl;

    }
    std::cout<<"are we here 295 main"<<std::endl;
    parser.stop();


    std::cout << "Hello, World!" << std::endl;
    return 0;
}
