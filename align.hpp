#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <iostream>

using namespace std;
#define MODELFILE "cvnip_fastalign.model"

class fast_align {
public:
    double lambda;
    bool use_null_word;
    unordered_map<string, unordered_map<string, double>> ttable;

    fast_align(double diagonal_tension = 4.0, bool use_null = true)
        : lambda(diagonal_tension), use_null_word(use_null) {}

    double diagonal_prob(int srci, int tgti, int slen, int tgtlen) const {
        double normsrc = static_cast<double>(srci) / slen;
        double normtgt = static_cast<double>(tgti) / tgtlen;
        return exp(-lambda * abs(normsrc - normtgt));
    }

    void normalize() {
        for (auto& [src, tgtmap] : ttable) {
            double tot = 0;
            for (auto& [tgt, v] : tgtmap) tot += v;
            if (tot > 0) {
                for (auto& [tgt, v] : tgtmap) v /= tot;
            }
        }
    }

    void train_pair(const vector<string>& src, const vector<string>& tgt, int iter = 5) {
        if (src.empty() || tgt.empty()) return;

        for (const auto& s : src)
            for (const auto& t : tgt)
                ttable[s][t] = 1.0 / tgt.size();

        if (use_null_word)
            for (const auto& t : tgt) ttable["<eps>"][t] = 1.0 / tgt.size();

        for (int it = 0; it < iter; ++it) {
            unordered_map<string, unordered_map<string, double>> counts;

            for (int j = 0; j < tgt.size(); ++j) {
                vector<double> probs(src.size() + 1, 0.0);
                double sum = 0.0;

                if (use_null_word) {
                    probs[0] = ttable["<eps>"][tgt[j]];
                    sum += probs[0];
                }

                for (int i = 0; i < src.size(); ++i) {
                    probs[i + 1] = ttable[src[i]][tgt[j]] * diagonal_prob(i, j, src.size(), tgt.size());
                    sum += probs[i + 1];
                }

                if (use_null_word) counts["<eps>"][tgt[j]] += probs[0] / sum;
                for (int i = 0; i < src.size(); ++i)
                    counts[src[i]][tgt[j]] += probs[i + 1] / sum;
            }

            for (auto& [src, tgtmap] : counts)
                for (auto& [tgt, val] : tgtmap)
                    ttable[src][tgt] += val;

            normalize();
        }
    }

    vector<pair<int, int>> align(const vector<string>& src, const vector<string>& tgt) {
        vector<pair<int, int>> alignment;
        int slen = src.size(), tlen = tgt.size();

        for (int j = 0; j < tlen; ++j) {
            double best = 0.0;
            int besti = -1;
            if (use_null_word) {
                double p = ttable.count("<eps>") && ttable["<eps>"].count(tgt[j]) ? ttable["<eps>"][tgt[j]] : 0.0;
                if (p > best) { best = p; besti = -1; }
            }
            for (int i = 0; i < slen; ++i) {
                double p = (ttable.count(src[i]) && ttable[src[i]].count(tgt[j]) ? ttable[src[i]][tgt[j]] : 1e-12)
                    * diagonal_prob(i, j, slen, tlen);
                if (p > best) { best = p; besti = i; }
            }

            if (besti >= 0) alignment.push_back({besti, j});
        }

        return alignment;
    }

    void savemodel() {
        ofstream out(MODELFILE);
        for (auto& [src, tgtmap] : ttable)
            for (auto& [tgt, prob] : tgtmap)
                out << src << "\t" << tgt << "\t" << prob << "\n";
    }

    void loadmodel() {
        ttable.clear();
        ifstream in(MODELFILE);
        string src, tgt;
        double prob;
        while (in >> src >> tgt >> prob)
            ttable[src][tgt] = prob;
    }
};
