import Image from "next/image";
import DownloadButton from "./DownloadButton";

export default function Hero() {
  return (
    <section className="relative flex flex-col items-center justify-center px-6 pt-24 pb-16 md:pt-32 md:pb-24">
      {/* Gradient background effect */}
      <div className="absolute inset-0 -z-10 overflow-hidden">
        <div className="absolute top-1/4 left-1/2 -translate-x-1/2 w-[600px] h-[600px] bg-purple-500/20 rounded-full blur-[120px]" />
        <div className="absolute top-1/3 left-1/3 w-[400px] h-[400px] bg-blue-500/10 rounded-full blur-[100px]" />
      </div>

      {/* Logo and title */}
      <div className="flex items-center gap-3 mb-6">
        <Image
          src="/icon.png"
          alt="PathView logo"
          width={48}
          height={48}
          className="rounded-lg"
        />
        <h1 className="text-4xl md:text-5xl font-bold text-white tracking-tight">
          PathView
        </h1>
      </div>

      {/* Tagline */}
      <p className="text-lg md:text-xl text-gray-400 text-center max-w-2xl mb-8">
        A high-performance whole-slide image viewer for digital pathology.
        <br className="hidden md:block" />
        GPU-accelerated rendering with AI agent integration.
      </p>

      {/* Download button */}
      <DownloadButton />

      {/* Demo GIF */}
      <div className="mt-12 md:mt-16 w-full max-w-4xl">
        <div className="relative rounded-xl overflow-hidden border border-gray-800 shadow-2xl shadow-purple-500/10">
          <Image
            src="/pathview.gif"
            alt="PathView demo"
            width={1200}
            height={675}
            className="w-full h-auto"
            unoptimized
            priority
          />
        </div>
      </div>
    </section>
  );
}
