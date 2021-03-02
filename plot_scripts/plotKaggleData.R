#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
filename <- 'kaggleCDF.pdf'
data<-read.csv('kaggle_data')


# add a line plot
plot <- ggplot(data, aes_string(x="size", y="cdf")) +
        geom_line(size=1, color="#377eb8") +
        scale_x_continuous(expand=c(0,0), trans="log2") +
        coord_cartesian(xlim=c(1,4000),ylim=c(0,1)) +
        labs(x="Size (€)", y="CDF") +

        theme_minimal(base_size=25) + 
        theme(legend.position="none")

ggsave(filename, width=5.7, height=4)
