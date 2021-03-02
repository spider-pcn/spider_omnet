#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
filename <- 'lndCapacityCDFJuly15min25.pdf'
data<-read.csv('lnd_july15_data_min25')

print(mean(data$values))

# add a line plot
plot <- ggplot(data, aes_string(x="values")) +
        stat_ecdf(size=1, geom='step', color="#377eb8") +
        #geom_density() +
        scale_x_continuous() +
        labs(x="Channel Size (€)", y="CDF") +

        theme_minimal(base_size=25) + 
        theme(legend.position="none")

ggsave(filename, width=5.7, height=4)
