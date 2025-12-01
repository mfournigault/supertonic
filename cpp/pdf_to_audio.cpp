#include "helper.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct PDFToAudioArgs {
    std::string pdf_path;
    std::string output_path;
    std::string voice_style = "../assets/voice_styles/M1.json";
    std::string onnx_dir = "../assets/onnx";
    std::string pdftotext_path;
    int first_page = -1;
    int last_page = -1;
    int total_step = 5;
    float speed = 1.05f;
    bool remove_footnotes = false;
    bool debug = false;
    bool verbose = true;
};

void printUsage(const char* program_name) {
    std::cout << "PDF to Audio Converter using Supertonic TTS\n\n"
              << "Usage: " << program_name << " --pdf <pdf_file> [options]\n\n"
              << "Required Arguments:\n"
              << "  --pdf <path>           Path to input PDF file\n\n"
              << "Optional Arguments:\n"
              << "  --output <path>        Output audio file path (default: <pdf_name>.wav)\n"
              << "  --first-page <num>     First page to read (default: all pages)\n"
              << "  --last-page <num>      Last page to read (default: all pages)\n"
              << "  --voice-style <path>   Voice style JSON file (default: ../assets/voice_styles/M1.json)\n"
              << "  --onnx-dir <path>      ONNX models directory (default: ../assets/onnx)\n"
              << "  --total-step <num>     Denoising steps, higher=better quality (default: 5)\n"
              << "  --speed <float>        Speech speed multiplier (default: 1.05, range: 0.9-1.5)\n"
              << "  --pdftotext-path <path> Path to pdftotext executable (default: $XPDF_HOME/bin64/pdftotext)\n"
              << "  --remove-footnotes     Remove footnotes, references, and citations from text\n"
              << "  --debug                Save extracted text to file for debugging\n"
              << "  --quiet                Suppress verbose output\n"
              << "  --help                 Show this help message\n\n"
              << "Environment Variables:\n"
              << "  XPDF_HOME              Base directory of xpdf-tools installation\n\n"
              << "Examples:\n"
              << "  # Convert entire PDF with default settings\n"
              << "  " << program_name << " --pdf document.pdf\n\n"
              << "  # Convert pages 10-20 with female voice\n"
              << "  " << program_name << " --pdf book.pdf --first-page 10 --last-page 20 \\\n"
              << "    --voice-style ../assets/voice_styles/F1.json --output chapter2.wav\n\n"
              << "  # High quality conversion with slower speed\n"
              << "  " << program_name << " --pdf article.pdf --total-step 10 --speed 0.95\n\n";
}

PDFToAudioArgs parseArgs(int argc, char* argv[]) {
    PDFToAudioArgs args;
    
    if (argc < 2) {
        printUsage(argv[0]);
        exit(1);
    }
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            exit(0);
        }
        else if (arg == "--pdf" && i + 1 < argc) {
            args.pdf_path = argv[++i];
        }
        else if (arg == "--output" && i + 1 < argc) {
            args.output_path = argv[++i];
        }
        else if (arg == "--first-page" && i + 1 < argc) {
            args.first_page = std::stoi(argv[++i]);
        }
        else if (arg == "--last-page" && i + 1 < argc) {
            args.last_page = std::stoi(argv[++i]);
        }
        else if (arg == "--voice-style" && i + 1 < argc) {
            args.voice_style = argv[++i];
        }
        else if (arg == "--onnx-dir" && i + 1 < argc) {
            args.onnx_dir = argv[++i];
        }
        else if (arg == "--total-step" && i + 1 < argc) {
            args.total_step = std::stoi(argv[++i]);
        }
        else if (arg == "--speed" && i + 1 < argc) {
            args.speed = std::stof(argv[++i]);
        }
        else if (arg == "--pdftotext-path" && i + 1 < argc) {
            args.pdftotext_path = argv[++i];
        }
        else if (arg == "--remove-footnotes") {
            args.remove_footnotes = true;
        }
        else if (arg == "--debug") {
            args.debug = true;
        }
        else if (arg == "--quiet") {
            args.verbose = false;
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n\n";
            printUsage(argv[0]);
            exit(1);
        }
    }
    
    // Validate required arguments
    if (args.pdf_path.empty()) {
        std::cerr << "Error: --pdf argument is required\n\n";
        printUsage(argv[0]);
        exit(1);
    }
    
    // Check if PDF file exists
    if (!fs::exists(args.pdf_path)) {
        std::cerr << "Error: PDF file not found: " << args.pdf_path << "\n";
        exit(1);
    }
    
    // Set default output path if not specified
    if (args.output_path.empty()) {
        fs::path pdf_path(args.pdf_path);
        args.output_path = pdf_path.stem().string() + ".wav";
    }
    
    return args;
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "=== PDF to Audio Converter ===\n\n";
        
        // --- 1. Parse arguments --- //
        PDFToAudioArgs args = parseArgs(argc, argv);
        
        if (args.verbose) {
            std::cout << "Configuration:\n"
                      << "  PDF file: " << args.pdf_path << "\n"
                      << "  Output: " << args.output_path << "\n";
            if (args.first_page > 0 || args.last_page > 0) {
                std::cout << "  Pages: ";
                if (args.first_page > 0) std::cout << args.first_page;
                else std::cout << "1";
                std::cout << " to ";
                if (args.last_page > 0) std::cout << args.last_page;
                else std::cout << "end";
                std::cout << "\n";
            }
            std::cout << "  Voice style: " << args.voice_style << "\n"
                      << "  Quality steps: " << args.total_step << "\n"
                      << "  Speed: " << args.speed << "x\n\n";
        }
        
        // --- 2. Extract text from PDF --- //
        std::string text_raw;
        std::string text;
        
        if (args.debug) {
            // Extract without cleaning first for debug
            text_raw = timer("Extracting text from PDF", [&]() {
                return extractTextFromPDF(
                    args.pdf_path,
                    args.first_page,
                    args.last_page,
                    args.pdftotext_path,
                    false  // No cleaning for raw output
                );
            });
            
            // Save raw extracted text
            fs::path pdf_path(args.pdf_path);
            std::string debug_file_raw = pdf_path.stem().string() + "_extracted_raw.txt";
            std::ofstream out_raw(debug_file_raw);
            out_raw << text_raw;
            out_raw.close();
            std::cout << "[DEBUG] Raw extracted text saved to: " << debug_file_raw << "\n";
            
            // Now apply cleaning if requested
            if (args.remove_footnotes) {
                text = cleanFootnotes(text_raw);
                std::string debug_file_clean = pdf_path.stem().string() + "_extracted_cleaned.txt";
                std::ofstream out_clean(debug_file_clean);
                out_clean << text;
                out_clean.close();
                std::cout << "[DEBUG] Cleaned text saved to: " << debug_file_clean << "\n";
            } else {
                text = text_raw;
            }
        } else {
            // Normal extraction
            text = timer("Extracting text from PDF", [&]() {
                return extractTextFromPDF(
                    args.pdf_path,
                    args.first_page,
                    args.last_page,
                    args.pdftotext_path,
                    args.remove_footnotes
                );
            });
        }
        
        if (args.verbose) {
            std::cout << "Extracted " << text.length() << " characters\n";
            // Show preview of extracted text
            std::string preview = text.substr(0, std::min<size_t>(200, text.length()));
            if (text.length() > 200) preview += "...";
            std::cout << "Preview: " << preview << "\n\n";
        }
        
        // --- 3. Initialize ONNX Runtime and load TTS model --- //
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "PDFToAudio");
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
            OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault
        );
        
        auto text_to_speech = loadTextToSpeech(env, args.onnx_dir, false);
        std::cout << std::endl;
        
        // --- 4. Load voice style --- //
        auto style = loadVoiceStyle({args.voice_style}, args.verbose);
        
        // --- 5. Synthesize speech (long-form mode with automatic chunking) --- //
        auto result = timer("Synthesizing speech from text", [&]() {
            return text_to_speech->call(
                memory_info,
                text,
                style,
                args.total_step,
                args.speed
            );
        });
        
        // --- 6. Extract audio and save to file --- //
        int sample_rate = text_to_speech->getSampleRate();
        int wav_len = static_cast<int>(sample_rate * result.duration[0]);
        
        std::vector<float> wav_out(
            result.wav.begin(),
            result.wav.begin() + wav_len
        );
        
        writeWavFile(args.output_path, wav_out, sample_rate);
        
        if (args.verbose) {
            float duration_sec = result.duration[0];
            int duration_min = static_cast<int>(duration_sec / 60);
            int duration_sec_remainder = static_cast<int>(duration_sec) % 60;
            
            std::cout << "\n=== Conversion completed successfully! ===\n"
                      << "Output file: " << args.output_path << "\n"
                      << "Duration: " << duration_min << "m " << duration_sec_remainder << "s\n"
                      << "Sample rate: " << sample_rate << " Hz\n";
        }
        
        clearTensorBuffers();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        return 1;
    }
}
