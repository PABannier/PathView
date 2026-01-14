import type { Metadata } from "next";
import { Geist, Geist_Mono } from "next/font/google";
import "./globals.css";

const geistSans = Geist({
  variable: "--font-geist-sans",
  subsets: ["latin"],
});

const geistMono = Geist_Mono({
  variable: "--font-geist-mono",
  subsets: ["latin"],
});

export const metadata: Metadata = {
  title: "PathView - High-Performance WSI Viewer for Digital Pathology",
  description:
    "A high-performance whole-slide image viewer for digital pathology. GPU-accelerated rendering with polygon overlays, tissue heatmaps, and AI agent integration via MCP.",
  keywords: [
    "digital pathology",
    "whole-slide image",
    "WSI viewer",
    "cell segmentation",
    "tissue analysis",
    "AI pathology",
    "MCP",
    "OpenSlide",
  ],
  authors: [{ name: "PABannier" }],
  openGraph: {
    title: "PathView - High-Performance WSI Viewer",
    description:
      "GPU-accelerated whole-slide image viewer for digital pathology with AI agent integration.",
    url: "https://pathview.vercel.app",
    siteName: "PathView",
    type: "website",
  },
  twitter: {
    card: "summary_large_image",
    title: "PathView - High-Performance WSI Viewer",
    description:
      "GPU-accelerated whole-slide image viewer for digital pathology with AI agent integration.",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en" className="dark">
      <body
        className={`${geistSans.variable} ${geistMono.variable} antialiased bg-gray-950 text-gray-100`}
      >
        {children}
      </body>
    </html>
  );
}
